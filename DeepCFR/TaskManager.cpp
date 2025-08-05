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
}

template<typename GameType>
void TaskManager<GameType>::stop() {
    if (m_stop_flag.exchange(true)) return;

    // Add empty tasks to unblock any waiting worker threads
    for(size_t i=0; i<m_workers.size(); ++i) {
        m_ready_queue.push(nullptr);
    }

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
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
    auto parent_s = task->parent_state;

    // Compute advantages (instantaneous regrets) for the current node
    std::vector<float> instant_regrets;
    instant_regrets.reserve(parent_s->legal_actions.size());
    for (float cfv : parent_s->counterfactual_values) {
        instant_regrets.push_back(cfv - parent_s->node_value);
    }

    // Store the computed advantage sample in memory
    TrainingSampleAdvantage sample;
        sample.infoset = { task->game_state.getCardTensors(task->game_state.getCurrentPlayer(), task->game_state.getCurrentRound()), task->game_state.getBetTensor() };
        sample.iteration = task->iter;
        sample.advantages = instant_regrets;
        for (const auto& action : parent_s->legal_actions)
        {
            sample.legal_action_indices.push_back(static_cast<int>(action));
        }
    add_advantage_sample(task->update_player, std::move(sample));

        // Report our node_value up to our own parent (if we have one)
    if (task->parent_state
        && task->parent_state->parent_task
        && task->parent_state->parent_task->parent_state) {
        // Get a reference to the grandparent's state object for clarity.
        auto grandparent_s = task->parent_state->parent_task->parent_state;

        // The value we are "returning" is the node_value we just calculated.
        float value_to_report_up = parent_s->node_value;

        // Lock the grandparent's state to modify it safely.
        std::lock_guard<std::mutex> lock(grandparent_s->mtx);

        // Store our result in the grandparent's data structures.
        grandparent_s->counterfactual_values[task->action_index_in_parent] = value_to_report_up;
        grandparent_s->node_value += grandparent_s->strategy[task->action_index_in_parent] * value_to_report_up;

        // Decrement the grandparent's child counter.
        // If we were the last child, the grandparent task is now complete and ready for processing.
        if (grandparent_s->children_to_complete.fetch_sub(1) - 1 == 0) {
            // The grandparent is now a completed parent. Re-queue it.
            grandparent_s->parent_task->type = Task<GameType>::Type::PROCESS_PARENT;
            submit_task(grandparent_s->parent_task);
        }
    } else {
        // If there's no grandparent, this task was the root of the traversal. We are done.
        ++m_traversals_completed;
        m_completion_cond.notify_all();
    }
    return;
}

    // ---- STATE 4: Decision Node (may need GPU) ----
    int current_player = task->game_state.getCurrentPlayer();

    // Create a new task for the continuation logic
    auto resume_task = std::make_unique<Task<GameType>>(*task);
    resume_task->type = Task<GameType>::Type::RESUME_AFTER_GPU;
    auto resume_task_ptr = resume_task.get(); // Get raw ptr before move

    // Create and submit the request to the GPU
    auto request = std::make_unique<ForwardRequest>();
    request->player_index = current_player;
    request->cards = task->game_state.getCardTensors(current_player, task->game_state.getCurrentRound());
    request->bets = task->game_state.getBetTensor();
    auto future = m_dispatcher.submit(std::move(request));

    // **THE CONTINUATION**
    // Spawn a lightweight waiter thread. This is the key to being non-blocking.
    // This thread's only job is to wait for the future and queue the resume_task.
    std::thread([this, f = std::move(future), r_task = std::move(resume_task)]() mutable {
        r_task->gpu_result = f.get(); // This blocks the waiter thread, not the worker
        this->submit_task(std::move(r_task));
    }).detach();


    // ---- STATE 5: Resuming after GPU ----
    if (task->type == Task<GameType>::Type::RESUME_AFTER_GPU) {
        auto advantages_tensor = task->gpu_result->to(torch::kCPU);
        auto legal_actions = task->game_state.getActions();
        std::vector<float> legal_advantages;
        for (const auto& action : legal_actions)
        {
            legal_advantages.push_back(advantages_tensor[0][static_cast<int>(action)].template item<float>());
        }
        // ... get legal advantages and compute strategy ...
        auto strategy = compute_strategy_from_advantages(legal_advantages);

        if (current_player == task->update_player) {
            // Traverser: explore all actions, create a parent state
            auto shared_task = std::make_shared<Task<GameType>>(std::move(*task));
            auto parent_s = std::make_shared<ParentState<GameType>>(shared_task, strategy, legal_actions);
            shared_task->parent_state = parent_s;

            for (size_t i = 0; i < legal_actions.size(); ++i) {
                GameType child_game_state = shared_task->game_state;
                child_game_state.transition(legal_actions[i]);
                auto child_task = std::make_unique<Task<GameType>>(Task<GameType>{
                    .type = Task<GameType>::Type::TRAVERSE_NODE,
                    .game_state = child_game_state, // 3. Use the new state
                    .update_player = shared_task->update_player,
                    .iter = shared_task->iter,
                    .parent_state = parent_s,
                    .action_index_in_parent = static_cast<int>(i)
                });

                submit_task(std::move(child_task));
            }
        } else {
            TrainingSampleStrategy strat_sample;
            strat_sample.infoset = { task->game_state.getCardTensors(current_player, task->game_state.getCurrentRound()), task->game_state.getBetTensor() };
            strat_sample.iteration = task->iter;
            strat_sample.strategy = strategy;
            strat_sample.weight = static_cast<float>(task->iter); // Or other weighting

            // Get legal action indices for the strategy sample
            strat_sample.legal_action_indices.reserve(legal_actions.size());
            for(const auto& action : legal_actions) {
                strat_sample.legal_action_indices.push_back(GameType::ActionMapping::getActionIndex(action));
            }

            add_strategy_sample(std::move(strat_sample));

            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx = dist(local_rng);

            auto next_task = std::make_unique<Task<GameType>>(*task); // copy parent info
            next_task->game_state.transition(legal_actions[action_idx]);
            submit_task(std::move(next_task));
        }
    }
}


// Explicit template instantiation for your game types need to be in the .cpp
template class TaskManager<Preflop::Game>;
template class TaskManager<Texas::Game>;

