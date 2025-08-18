// DeepCFRCoroutine.hpp
#ifndef DEEPCFR_COROUTINE_HPP
#define DEEPCFR_COROUTINE_HPP

#include <coroutine>
#include <memory>
#include <queue>
#include <deque>
#include <future>
#include "types.hpp"
#include "GPUDispatcher.hpp"
#include "DeepCFR.hpp"

template<typename GameType>
class DeepCFRCoroutine : public DeepRegretMinimizer<GameType> {
public:
    using Base = DeepRegretMinimizer<GameType>;
    using Base::m_rng;
    using Base::m_game;
    using Base::m_device;
    using Base::m_advantage_networks;
    using Base::m_adv_memories;
    using Base::m_strategy_memory;
    using Base::compute_strategy_from_advantages;
    using Base::add_to_strategy_memory;

    // Forward declaration of coroutine return type
    struct TraversalTask;

    // Awaitable for GPU operations
    struct GPUAwaiter {
        std::future<torch::Tensor> future;

        bool await_ready() {
            return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

        void await_suspend(std::coroutine_handle<> h) {
            // Handle is stored by the scheduler, not here
        }

        torch::Tensor await_resume() {
            return future.get();
        }
    };

    // Coroutine promise type
    struct TraversalPromise {
        float returned_value = 0.0f;
        std::exception_ptr exception;

        TraversalTask get_return_object();

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(float value) {
            returned_value = value;
        }

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    // Coroutine handle wrapper
    struct TraversalTask {
        using promise_type = TraversalPromise;
        using handle_type = std::coroutine_handle<promise_type>;

        handle_type coro;

        explicit TraversalTask(handle_type h) : coro(h) {}

        ~TraversalTask() {
            if (coro) coro.destroy();
        }

        TraversalTask(TraversalTask&& other) noexcept
            : coro(std::exchange(other.coro, {})) {}

        TraversalTask& operator=(TraversalTask&& other) noexcept {
            if (this != &other) {
                if (coro) coro.destroy();
                coro = std::exchange(other.coro, {});
            }
            return *this;
        }

        bool done() const { return coro.done(); }
        void resume() { coro.resume(); }
        float get_value() const { return coro.promise().return_value; }
    };

    explicit DeepCFRCoroutine(uint32_t seed = std::random_device()())
        : Base(seed), m_gpu_dispatcher(m_advantage_networks, m_device) {
        m_gpu_dispatcher.start();
    }

    ~DeepCFRCoroutine() {
        m_gpu_dispatcher.stop();
    }

    void TrainWithCoroutines(uint32_t iterations, size_t max_concurrent = 128) {
        for (uint32_t iter = 1; iter <= iterations; ++iter) {
            std::cout << "Iteration " << iter << "/" << iterations << std::endl;

            for (int p = 0; p < GameType::PlayerNum; ++p) {
                auto t1 = std::chrono::high_resolution_clock::now();

                // Move networks to CPU for traversal
                m_advantage_networks[0]->to(torch::kCPU);
                m_advantage_networks[1]->to(torch::kCPU);

                // Run coroutine traversals
                run_coroutine_scheduler(p, iter, max_concurrent);

                auto t2 = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
                std::cout << "Coroutine training for player " << p
                         << " time: " << ms.count() << "ms" << std::endl;

                m_game.reInitialize();

                // Move network back to GPU for training
                m_advantage_networks[p]->to(m_device);
                Base::train_advantage_network(p);
            }
        }

        Base::train_strategy_network();
    }

private:
    GPUDispatcher m_gpu_dispatcher;

    // Main coroutine traversal function
    TraversalTask traverse_cfr_coro(GameType game, int updatePlayer,
                                    int current_iter, float probUpdatePlayer) {
        // Terminal node
        if (game.getType() == "terminal") {
            co_return game.getUtility(updatePlayer);
        }

        // Chance node
        if (game.getType() == "chance") {
            game.transition(GameType::Action::Chance);
            float value = co_await traverse_cfr_coro(std::move(game), updatePlayer,
                                                     current_iter, probUpdatePlayer);
            co_return value;
        }

        // Decision node
        int currentPlayer = game.getCurrentPlayer();
        auto legal_actions = game.getActions();
        std::vector<int> legal_indices;

        for (auto action : legal_actions) {
            legal_indices.push_back(GameType::ActionMapping::getActionIndex(action));
        }

        // Get tensors for GPU
        auto cards_cpu = game.getCardTensors(currentPlayer, game.getCurrentRound());
        auto bets_cpu = game.getBetTensor();

        // Submit GPU request and await result
        auto request = std::make_unique<ForwardRequest>();
        request->player_index = currentPlayer;
        request->cards = std::vector<torch::Tensor>{cards_cpu};
        request->bets = bets_cpu;

        auto future = m_gpu_dispatcher.submit(std::move(request));
        auto advantages_tensor = co_await GPUAwaiter{std::move(future)};

        // Extract legal advantages
        std::vector<float> legal_advantages;
        for (int idx : legal_indices) {
            legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
        }

        // Compute strategy
        auto strategy = compute_strategy_from_advantages(legal_advantages);

        float nodeValue = 0.0f;

        if (currentPlayer == updatePlayer) {
            // Traverser: explore all actions
            std::vector<float> counterfactualValues(legal_actions.size());

            // Launch all child traversals concurrently
            std::vector<TraversalTask> child_tasks;
            child_tasks.reserve(legal_actions.size());

            for (size_t a = 0; a < legal_actions.size(); ++a) {
                GameType next_game(game);
                next_game.transition(legal_actions[a]);

                child_tasks.push_back(
                    traverse_cfr_coro(std::move(next_game), updatePlayer,
                                     current_iter, probUpdatePlayer * strategy[a])
                );
            }

            // Await all child results
            for (size_t a = 0; a < legal_actions.size(); ++a) {
                // Resume child coroutines until complete
                while (!child_tasks[a].done()) {
                    child_tasks[a].resume();
                    if (!child_tasks[a].done()) {
                        // Child is waiting for GPU, yield control
                        co_await std::suspend_always{};
                    }
                }

                counterfactualValues[a] = child_tasks[a].get_value();
                nodeValue += strategy[a] * counterfactualValues[a];
            }

            // Compute and store advantages
            std::vector<float> instant_regrets(legal_actions.size());
            for (size_t a = 0; a < legal_actions.size(); ++a) {
                instant_regrets[a] = counterfactualValues[a] - nodeValue;
            }

            TrainingSampleAdvantage sample;
            sample.infoset = {std::vector<torch::Tensor>{cards_cpu}, bets_cpu};
            sample.iteration = current_iter;
            sample.legal_action_indices = legal_indices;
            sample.advantages = instant_regrets;
            sample.weight = static_cast<float>(current_iter);

            m_adv_memories[updatePlayer].add_sample(sample, m_rng);

        } else {
            // Opponent: sample action and store strategy
            TrainingSampleStrategy sample;
            sample.infoset = {std::vector<torch::Tensor>{cards_cpu}, bets_cpu};
            sample.iteration = current_iter;
            sample.legal_action_indices = legal_indices;
            sample.strategy = strategy;
            sample.weight = static_cast<float>(current_iter);

            add_to_strategy_memory(m_strategy_memory, Base::MEMORY_SIZE);

            // Sample action according to strategy
            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx = dist(m_rng);

            game.transition(legal_actions[action_idx]);

            // Single child traversal
            float child_value = co_await traverse_cfr_coro(
                std::move(game), updatePlayer, current_iter, probUpdatePlayer
            );

            nodeValue = child_value;
        }

        co_return nodeValue;
    }

    // Scheduler that manages multiple concurrent traversals
    void run_coroutine_scheduler(int player, int iteration, size_t max_concurrent) {
        struct ActiveTraversal {
            TraversalTask task;
            std::chrono::steady_clock::time_point start_time;
            bool is_root;
        };

        std::deque<std::unique_ptr<ActiveTraversal>> active_traversals;
        std::queue<std::unique_ptr<ActiveTraversal>> ready_queue;
        int completed_traversals = 0;
        const int target_traversals = Base::K_TRAVERSALS;

        // Lambda to start a new root traversal
        auto start_new_traversal = [&]() {
            GameType game(m_rng);
            auto traversal = std::make_unique<ActiveTraversal>();
            traversal->task = traverse_cfr_coro(std::move(game), player, iteration, 1.0f);
            traversal->start_time = std::chrono::steady_clock::now();
            traversal->is_root = true;

            // Initial resume to start the coroutine
            traversal->task.resume();

            if (!traversal->task.done()) {
                active_traversals.push_back(std::move(traversal));
            } else {
                completed_traversals++;
            }
        };

        // Start initial batch
        for (size_t i = 0; i < std::min(max_concurrent, static_cast<size_t>(target_traversals)); ++i) {
            start_new_traversal();
        }

        // Main scheduler loop
        while (completed_traversals < target_traversals || !active_traversals.empty()) {
            bool made_progress = false;

            // Check each active traversal
            for (auto it = active_traversals.begin(); it != active_traversals.end(); ) {
                auto& traversal = *it;

                // Try to resume the coroutine
                if (!traversal->task.done()) {
                    traversal->task.resume();
                }

                if (traversal->task.done()) {
                    // Traversal completed
                    if (traversal->is_root) {
                        completed_traversals++;

                        // Start new traversal if needed
                        if (completed_traversals < target_traversals) {
                            start_new_traversal();
                        }
                    }

                    it = active_traversals.erase(it);
                    made_progress = true;
                } else {
                    ++it;
                }
            }

            // Small yield to prevent busy waiting if no progress
            if (!made_progress && !active_traversals.empty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }

        std::cout << "Completed " << completed_traversals << " traversals" << std::endl;
    }
};

// Implementation of get_return_object (must be after TraversalTask is fully defined)
template<typename GameType>
inline typename DeepCFRCoroutine<GameType>::TraversalTask
DeepCFRCoroutine<GameType>::TraversalPromise::get_return_object() {
    return TraversalTask{TraversalTask::handle_type::from_promise(*this)};
}

#endif // DEEPCFR_COROUTINE_HPP