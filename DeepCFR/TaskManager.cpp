//
// Created by elijah on 8/5/25.
//

#include "TaskManager.hpp"
#include <iostream>

// Include your utility and compute_strategy functions
std::vector<float> compute_strategy_from_advantages(const std::vector<float>& advantages);
template<typename T>
void add_to_strategy_memory(std::vector<T>& memory, const T& sample, size_t max_size, std::mt19937& rng);


template<typename GameType>
TaskManager<GameType>::TaskManager(
    size_t num_threads, GPUDispatcher& dispatcher,
    std::array<AdvantageMemoryBuffer<GameType>, 2>& adv_memories,
    std::vector<TrainingSampleStrategy>& strat_memory,
    std::mt19937& main_rng
) : m_num_threads(num_threads), m_dispatcher(dispatcher),
    m_adv_memories(adv_memories), m_strategy_memory(strat_memory), m_rng(main_rng) {}

template<typename GameType>
TaskManager<GameType>::~TaskManager() {
    stop();
}

template<typename GameType>
void TaskManager<GameType>::start() {
    for (size_t i = 0; i < m_num_threads; ++i) {
        m_workers.emplace_back(&TaskManager<GameType>::worker_loop, this);
    }
    m_continuation_thread = std::thread(&TaskManager<GameType>::continuation_loop, this);
}

template<typename GameType>
void TaskManager<GameType>::stop() {
    if (m_stop_flag.exchange(true)) return;

    // Unblock workers
    for(size_t i=0; i < m_workers.size(); ++i) {
        m_ready_queue.push(nullptr);
    }

    // Unblock the continuation thread by pushing a sentinel value
    // (A default-constructed future and a nullptr task)
    m_pending_futures_queue.push({});

    // Join all threads
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    // Join the continuation thread
    if (m_continuation_thread.joinable()) {
        m_continuation_thread.join();
    }
}

template<typename GameType>
void TaskManager<GameType>::continuation_loop() {
    while (!m_stop_flag) {
        // This call blocks and returns the std::pair directly.
        auto future_task_pair = m_pending_futures_queue.pop();

        // Use a structured binding to unpack the pair.
        auto& [future, task] = future_task_pair;

        // The sentinel value to stop the loop is a null task pointer.
        // This is the correct way to check for the stop signal.
        if (!task) {
            break;
        }

        // Block *this* thread until the GPU result is ready.
        task->gpu_result = future.get();

        // Re-submit the task with the GPU result to the main work queue.
        this->submit_task(std::move(task));
    }
}

template<typename GameType>
void TaskManager<GameType>::submit_task(std::shared_ptr<Task<GameType>> task) {
    m_ready_queue.push(std::move(task));
}

template<typename GameType>
void TaskManager<GameType>::wait_for_completion(int num_traversals) {
    std::unique_lock<std::mutex> lock(m_completion_mutex);
    m_completion_cond.wait(lock, [this, num_traversals] {
        return m_traversals_completed.load() >= num_traversals;
    });
}

template<typename GameType>
void TaskManager<GameType>::worker_loop() {
    std::mt19937 local_rng(std::random_device{}()); // Each thread gets its own RNG
    while (!m_stop_flag) {
        auto task = m_ready_queue.pop();
        if (task == nullptr) { // Sentinel value to stop
            break;
        }
        process_task(std::move(task), local_rng);
    }
}

template<typename GameType>
void TaskManager<GameType>::add_advantage_sample(int player, TrainingSampleAdvantage&& sample) {
    std::lock_guard<std::mutex> lock(m_memory_mutex);
    m_adv_memories[player].add_sample(sample, m_rng);
}

template<typename GameType>
void TaskManager<GameType>::add_strategy_sample(TrainingSampleStrategy&& sample) {
    std::lock_guard<std::mutex> lock(m_memory_mutex);
    // Assumes existence of this helper function
    add_to_strategy_memory(m_strategy_memory, sample, MEMORY_SIZE, m_rng);
}

template<typename GameType>
void TaskManager<GameType>::process_task(std::shared_ptr<Task<GameType>> task, std::mt19937& local_rng) {
    // ---- STATE 1: Terminal Node ----
    if (task->game_state.getType() == "terminal") {
        float utility = task->game_state.getUtility(task->update_player);
        if (task->parent_state) {
            auto parent_s = task->parent_state;
            std::lock_guard<std::mutex> lock(parent_s->mtx);
            parent_s->counterfactual_values[task->action_index_in_parent] = utility;
            parent_s->node_value += parent_s->strategy[task->action_index_in_parent] * utility;

            if (parent_s->children_to_complete.fetch_sub(1) - 1 == 0) {
                // This was the last child, re-queue the parent for processing
                parent_s->parent_task->type = Task<GameType>::Type::PROCESS_PARENT;
                submit_task(std::move(parent_s->parent_task));
            }
        } else {
            // This is the root of a traversal that was terminal
            ++m_traversals_completed;
            m_completion_cond.notify_all();
        }
        return;
    }

    // ---- STATE 2: Chance Node ----
    if (task->game_state.getType() == "chance") {
        auto next_task = std::make_unique<Task<GameType>>(*task); // Copy parent/iter state
        next_task->game_state.transition(GameType::Action::Chance);
        submit_task(std::move(next_task));
        return;
    }

    // ---- STATE 3: Parent Node (Finished Children) ----
    if (task->type == Task<GameType>::Type::PROCESS_PARENT) {
        auto parent_s = task->parent_state; // This is the state object for the current task.

        // Compute advantages (instantaneous regrets) for the current node
        std::vector<float> instant_regrets;
        instant_regrets.reserve(parent_s->legal_actions.size());
        for (float cfv : parent_s->counterfactual_values) {
            instant_regrets.push_back(cfv - parent_s->node_value);
        }

        // Store the computed advantage sample in memory
        TrainingSampleAdvantage sample;
        sample.infoset = { task->game_state.getCardTensors(task->game_state.getCurrentPlayer(), task->game_state.getCurrentRound()), task->game_state.getBetTensor() };
        sample.advantages = instant_regrets;
        sample.weight = static_cast<float>(task->iter);
        for (const auto& action : parent_s->legal_actions) {
            sample.legal_action_indices.push_back(static_cast<int>(action));
        }
        add_advantage_sample(task->update_player, std::move(sample));

        auto grandparent_s = parent_s->grandparent_state;
        if (grandparent_s) {
            // This node has a parent in the tree to report its value to.
            float value_to_report_up = parent_s->node_value;
            int my_action_index = parent_s->parent_action_index_in_grandparent;

            std::lock_guard<std::mutex> lock(grandparent_s->mtx);

            grandparent_s->counterfactual_values[my_action_index] = value_to_report_up;
            grandparent_s->node_value += grandparent_s->strategy[my_action_index] * value_to_report_up;

            if (grandparent_s->children_to_complete.fetch_sub(1) - 1 == 0) {
                // The grandparent is now complete, so re-queue it for processing.
                grandparent_s->parent_task->type = Task<GameType>::Type::PROCESS_PARENT;
                submit_task(grandparent_s->parent_task);
            }
        } else {
            // This node was the root of the traversal. Mark it as complete.
            ++m_traversals_completed;
            m_completion_cond.notify_all();
        }
        return;
    }


    // ---- STATE 4: Resuming after GPU ----
    if (task->type == Task<GameType>::Type::RESUME_AFTER_GPU) {
        auto advantages_tensor = task->gpu_result->to(torch::kCPU);
        auto legal_actions = task->game_state.getActions();
        auto current_player = task->game_state.getCurrentPlayer();

        std::vector<float> legal_advantages;
        std::vector<int> legal_indices; // Needed for strategy sample
        for (const auto& action : legal_actions) {
            int action_idx = GameType::ActionMapping::getActionIndex(action);
            legal_advantages.push_back(advantages_tensor[0][action_idx].template item<float>());
            legal_indices.push_back(action_idx);
        }

        auto strategy = compute_strategy_from_advantages(legal_advantages);

        if (current_player == task->update_player) {
            // Traverser: explore all actions, create a parent state
            auto shared_task = std::make_shared<Task<GameType>>(std::move(*task));
            auto parent_s = std::make_shared<ParentState<GameType>>(shared_task, strategy, legal_actions);
            // Preserve the link to the grandparent's state before overwriting ---
            parent_s->grandparent_state = shared_task->parent_state;
            parent_s->parent_action_index_in_grandparent = shared_task->action_index_in_parent;

            shared_task->parent_state = parent_s;

            for (size_t i = 0; i < legal_actions.size(); ++i) {
                GameType child_game_state = shared_task->game_state;
                child_game_state.transition(legal_actions[i]);
                auto child_task = std::make_unique<Task<GameType>>(Task<GameType>{
                    .type = Task<GameType>::Type::TRAVERSE_NODE,
                    .game_state = child_game_state,
                    .update_player = shared_task->update_player,
                    .iter = shared_task->iter,
                    .parent_state = parent_s,
                    .action_index_in_parent = static_cast<int>(i)
                });
                submit_task(std::move(child_task));
            }
        } else {
            // Opponent: sample one action, store strategy
            TrainingSampleStrategy strat_sample;
            strat_sample.infoset = { task->game_state.getCardTensors(current_player, task->game_state.getCurrentRound()), task->game_state.getBetTensor() };
            strat_sample.strategy = strategy;
            strat_sample.legal_action_indices = legal_indices;
            strat_sample.weight = static_cast<float>(task->iter);
            add_strategy_sample(std::move(strat_sample));

            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx_in_legal = dist(local_rng);

            auto next_task = std::make_unique<Task<GameType>>(*task);
            next_task->type = Task<GameType>::Type::TRAVERSE_NODE; // Ensure next task is for traversal
            next_task->gpu_result.reset(); // Clear the GPU result
            next_task->game_state.transition(legal_actions[action_idx_in_legal]);
            submit_task(std::move(next_task));
        }
        return;
    }


    // ---- STATE 5: Decision Node (Needs GPU) ----
    // This now only handles TRAVERSE_NODE for decision nodes
    if (task->type == Task<GameType>::Type::TRAVERSE_NODE) {
        int current_player = task->game_state.getCurrentPlayer();

        // Create a task for the continuation logic
        auto resume_task = std::make_shared<Task<GameType>>(*task);
        resume_task->type = Task<GameType>::Type::RESUME_AFTER_GPU;

        // Create and submit the request to the GPU
        auto request = std::make_unique<ForwardRequest>();
        request->player_index = current_player;
        request->cards = task->game_state.getCardTensors(current_player, task->game_state.getCurrentRound());
        request->bets = task->game_state.getBetTensor();
        auto future = m_dispatcher.submit(std::move(request));

        m_pending_futures_queue.push({std::move(future), std::move(resume_task)});
    }
}



// Explicit template instantiation for your game types need to be in the .cpp
template class TaskManager<Preflop::Game>;
template class TaskManager<Texas::Game>;

