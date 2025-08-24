#ifndef COROUTINE_TRAVERSAL_HPP
#define COROUTINE_TRAVERSAL_HPP

#include <coroutine>
#include <memory>
#include <queue>
#include <deque>
#include <future>
#include <optional>
#include <vector>
#include <chrono>
#include <random>
#include <atomic>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <torch/torch.h>
#include "Net.hpp"
#include "types.hpp"

// Forward declarations
template<typename GameType> class TraversalScheduler;
template<typename GameType> class TraversalTask;

// GPU request with coroutine integration
template<typename GameType>
struct GPURequest {
    std::vector<torch::Tensor> cards;
    torch::Tensor bets;
    std::coroutine_handle<> continuation;  // Coroutine to resume when ready
    torch::Tensor result;                  // Filled when computation complete
    std::atomic<bool> ready{false};        // Use atomic for thread safety
    int player_id = -1;                    // Track which player this is for
    int session_id = -1;                   // Track which training session
};

// Awaitable for GPU results
template<typename GameType>
class GPUAwaitable {
public:
    GPUAwaitable(std::shared_ptr<GPURequest<GameType>> req)
        : request(req) {}

    bool await_ready() const noexcept {
        return request->ready.load();  // Don't suspend if result already available
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept {
        // Store handle for later resumption
        request->continuation = h;

        // Double-check if result became ready while we were storing the handle
        // Return false to resume immediately if ready, true to stay suspended
        return !request->ready.load();
    }

    torch::Tensor await_resume() noexcept {
        // Clear the continuation to prevent double-resume
        request->continuation = nullptr;
        return request->result;
    }

private:
    std::shared_ptr<GPURequest<GameType>> request;
};

// Task type for coroutine traversals
template<typename GameType>
class TraversalTask {
public:
    struct promise_type {
        TraversalScheduler<GameType>* scheduler = nullptr;
        float result_value = 0.0f;
        std::exception_ptr exception;

        TraversalTask get_return_object() {
            return TraversalTask{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(float value) { result_value = value; }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    TraversalTask(handle_type h) : coro_handle(h) {}

    ~TraversalTask() {
        if (coro_handle)
            coro_handle.destroy();
    }

    // Move-only
    TraversalTask(TraversalTask&& other) noexcept
        : coro_handle(std::exchange(other.coro_handle, {})) {}

    TraversalTask& operator=(TraversalTask&& other) noexcept {
        if (this != &other) {
            if (coro_handle)
                coro_handle.destroy();
            coro_handle = std::exchange(other.coro_handle, {});
        }
        return *this;
    }

    float get_result() {
        if (!coro_handle.done())
            coro_handle.resume();

        if (coro_handle.promise().exception)
            std::rethrow_exception(coro_handle.promise().exception);

        return coro_handle.promise().result_value;
    }

    handle_type handle() const { return coro_handle; }

private:
    handle_type coro_handle;
};

// GPU Batch Processor - runs on separate thread
template<typename GameType>
class GPUBatchProcessor {
public:
    GPUBatchProcessor(std::array<DeepCFRModel, 2>& networks, torch::Device device)
        : m_networks(networks), m_device(device), m_stop(false)
    {
        for (auto network : networks) {
            network->to(m_device);
        }
    }

    ~GPUBatchProcessor() {
        stop();
    }

    void start() {
        m_thread = std::thread(&GPUBatchProcessor::process_loop, this);
    }

    void stop() {
        m_stop = true;
        m_cv.notify_all();
        if (m_thread.joinable())
            m_thread.join();
    }

    void submit(std::shared_ptr<GPURequest<GameType>> request, int player) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending[player].push_back(request);
        }
        m_cv.notify_one();
    }

    std::vector<std::shared_ptr<GPURequest<GameType>>> get_ready_requests() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<GPURequest<GameType>>> ready;
        ready.swap(m_completed);
        return ready;
    }

    void clear_pending_for_player(int player) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending[player].clear();
    }

private:
    void process_loop() {
        while (!m_stop) {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Wait for work or timeout for batching
            m_cv.wait_for(lock, std::chrono::milliseconds(5), [this] {
                return m_stop || has_pending_work();
            });

            // Process batches for each player
            for (int p = 0; p < 2; ++p) {
                if (!m_pending[p].empty()) {
                    auto batch = std::move(m_pending[p]);
                    m_pending[p].clear();
                    lock.unlock();

                    process_batch(batch, p);

                    lock.lock();
                    m_completed.insert(m_completed.end(), batch.begin(), batch.end());
                }
            }
        }
    }

    bool has_pending_work() {
        return !m_pending[0].empty() || !m_pending[1].empty();
    }

    void process_batch(std::vector<std::shared_ptr<GPURequest<GameType>>>& batch, int player) {
        if (batch.empty()) return;

        size_t batch_size = batch.size();
        int num_card_types = batch[0]->cards.size();

        // Prepare batched tensors
        std::vector<torch::Tensor> batched_cards;
        std::vector<torch::Tensor> bet_list;

        for (int i = 0; i < num_card_types; ++i) {
            std::vector<torch::Tensor> card_type_list;
            for (const auto& req : batch) {
                card_type_list.push_back(req->cards[i]);
            }
            batched_cards.push_back(torch::stack(card_type_list).squeeze(1).to(m_device));
        }

        for (const auto& req : batch) {
            bet_list.push_back(req->bets);
        }
        auto batched_bets = torch::stack(bet_list).squeeze(1).to(m_device);

        // Run inference
        torch::Tensor results;
        {
            torch::NoGradGuard no_grad;
            results = m_networks[player]->forward(batched_cards, batched_bets);
        }

        // Store results and mark as ready
        for (size_t i = 0; i < batch_size; ++i) {
            batch[i]->result = results.slice(0, i, i + 1);
            batch[i]->ready.store(true);  // Use atomic store
        }
    }

    std::array<DeepCFRModel, 2>& m_networks;
    torch::Device m_device;

    std::thread m_thread;
    std::atomic<bool> m_stop;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::array<std::vector<std::shared_ptr<GPURequest<GameType>>>, 2> m_pending;
    std::vector<std::shared_ptr<GPURequest<GameType>>> m_completed;
};

// Main scheduler for managing coroutine traversals
template<typename GameType>
class TraversalScheduler {
public:
    TraversalScheduler(std::array<DeepCFRModel, 2>& networks,
                      torch::Device device,
                      uint32_t seed = std::random_device()())
        : m_gpu_processor(networks, device),
          m_rng(seed),
          m_networks(networks) {
        m_gpu_processor.start();
    }

    // Coroutine traversal function
    TraversalTask<GameType> traverse_cfr_coro(const GameType& game,
                                              int update_player,
                                              int iteration,
                                              float prob_update_player,
                                              int session_id = -1) {
        // Terminal node
        if (game.getType() == "terminal") {
            co_return game.getUtility(update_player);
        }

        // Chance node
        if (game.getType() == "chance") {
            GameType next_game(game);
            next_game.transition(GameType::Action::Chance);
            auto task = traverse_cfr_coro(next_game, update_player, iteration, prob_update_player, session_id);
            float result = task.get_result();
            co_return result;
        }

        int current_player = game.getCurrentPlayer();
        auto legal_actions = game.getActions();
        std::vector<int> legal_indices;
        for (auto action : legal_actions) {
            legal_indices.push_back(GameType::ActionMapping::getActionIndex(action));
        }

        // Prepare GPU request
        auto gpu_request = std::make_shared<GPURequest<GameType>>();
        gpu_request->cards = game.getCardTensors(current_player, game.getCurrentRound());
        gpu_request->bets = game.getBetTensor();
        gpu_request->player_id = current_player;
        gpu_request->session_id = session_id;

        // Submit to GPU and suspend
        m_gpu_processor.submit(gpu_request, current_player);

        // Await GPU result - this suspends the coroutine
        torch::Tensor advantages_tensor = co_await GPUAwaitable<GameType>(gpu_request);

        // Extract legal advantages
        std::vector<float> legal_advantages;
        for (int idx : legal_indices) {
            legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
        }

        // Compute strategy
        auto strategy = compute_strategy_from_advantages(legal_advantages);

        float node_value = 0.0f;

        if (current_player == update_player) {
            // Traverser: explore all actions
            std::vector<float> counterfactual_values(legal_actions.size());

            for (size_t a = 0; a < legal_actions.size(); ++a) {
                GameType next_game(game);
                next_game.transition(legal_actions[a]);

                auto task = traverse_cfr_coro(next_game, update_player,
                                             iteration, prob_update_player * strategy[a], session_id);
                counterfactual_values[a] = task.get_result();
                node_value += strategy[a] * counterfactual_values[a];
            }

            // Store advantages in memory
            store_advantages(gpu_request->cards, gpu_request->bets,
                           legal_indices, counterfactual_values,
                           node_value, iteration, update_player);
        } else {
            // Opponent: sample single action
            store_strategy(gpu_request->cards, gpu_request->bets,
                         legal_indices, strategy, iteration);

            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx = dist(m_rng);
            GameType next_game(game);
            next_game.transition(legal_actions[action_idx]);

            auto task = traverse_cfr_coro(next_game, update_player,
                                         iteration, prob_update_player, session_id);
            node_value = task.get_result();
        }

        co_return node_value;
    }

    // Main training loop
    void train(uint32_t iterations, int traversals_per_iteration) {
        for (uint32_t iter = 1; iter <= iterations; ++iter) {
            std::cout << "Iteration " << iter << "/" << iterations << std::endl;

            for (int p = 0; p < GameType::PlayerNum; ++p) {
                run_traversals_for_player(p, iter, traversals_per_iteration);
            }
        }
    }

private:
    void run_traversals_for_player(int player, int iteration, int num_traversals) {
        // Generate unique session ID for this batch of traversals
        static std::atomic<int> session_counter{0};
        int session_id = session_counter.fetch_add(1);

        // Use shared_ptr to manage traversal lifetime
        std::vector<std::shared_ptr<TraversalTask<GameType>>> active_traversals;
        std::unordered_map<void*, std::shared_ptr<TraversalTask<GameType>>> handle_to_task;

        int completed = 0;
        int started = 0;

        auto start_time = std::chrono::high_resolution_clock::now();

        while (completed < num_traversals) {
            // Check for ready GPU results and resume corresponding coroutines
            auto ready_requests = m_gpu_processor.get_ready_requests();
            for (auto& req : ready_requests) {
                // Only process requests from this session
                if (req->session_id != session_id) {
                    continue;
                }

                // Check if continuation is valid and not null
                if (req->continuation && req->continuation.address()) {
                    // Store the handle temporarily
                    auto handle = req->continuation;

                    // Clear the continuation to avoid double-resume
                    req->continuation = nullptr;

                    // Find the task that owns this continuation
                    auto it = handle_to_task.find(handle.address());
                    if (it != handle_to_task.end() && !handle.done()) {
                        // Resume the coroutine
                        handle.resume();

                        // Check if completed after resume
                        if (handle.done()) {
                            completed++;
                            // Remove from active traversals
                            active_traversals.erase(
                                std::remove(active_traversals.begin(), active_traversals.end(), it->second),
                                active_traversals.end()
                            );
                            handle_to_task.erase(it);
                        }
                    }
                }
            }

            // Start new traversals if we haven't started enough
            if (started < num_traversals && active_traversals.size() < MAX_CONCURRENT) {
                GameType game(m_rng);
                auto task = std::make_shared<TraversalTask<GameType>>(
                    traverse_cfr_coro(game, player, iteration, 1.0f, session_id)
                );

                active_traversals.push_back(task);
                handle_to_task[task->handle().address()] = task;

                // Initial resume to start the traversal
                if (!task->handle().done()) {
                    task->handle().resume();
                    if (task->handle().done()) {
                        completed++;
                        active_traversals.pop_back();
                        handle_to_task.erase(task->handle().address());
                    }
                }
                started++;
            }

            // Small yield to prevent busy-waiting
            if (ready_requests.empty() && started >= num_traversals) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }

        // Wait for any remaining GPU requests for this session to complete
        // This ensures we don't leak requests between iterations
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Player " << player << " completed " << num_traversals
                  << " traversals in " << duration.count() << "ms" << std::endl;
    }

    std::vector<float> compute_strategy_from_advantages(const std::vector<float>& advantages) {
        std::vector<float> strategy(advantages.size());
        std::vector<float> positive_regrets(advantages.size());
        float sum_positive = 0.0f;

        for (size_t i = 0; i < advantages.size(); ++i) {
            positive_regrets[i] = std::max(0.0f, advantages[i]);
            sum_positive += positive_regrets[i];
        }

        if (sum_positive > 0) {
            for (size_t i = 0; i < advantages.size(); ++i) {
                strategy[i] = positive_regrets[i] / sum_positive;
            }
        } else {
            float uniform_prob = 1.0f / advantages.size();
            std::fill(strategy.begin(), strategy.end(), uniform_prob);
        }

        return strategy;
    }

    void store_advantages(const std::vector<torch::Tensor>& cards,
                         const torch::Tensor& bets,
                         const std::vector<int>& legal_indices,
                         const std::vector<float>& counterfactual_values,
                         float node_value,
                         int iteration,
                         int player) {
        // Implementation would store in advantage memory buffer
        // Similar to original code's m_adv_memories[player].add_sample()
    }

    void store_strategy(const std::vector<torch::Tensor>& cards,
                       const torch::Tensor& bets,
                       const std::vector<int>& legal_indices,
                       const std::vector<float>& strategy,
                       int iteration) {
        // Implementation would store in strategy memory buffer
        // Similar to original code's m_strategy_memory
    }

    static constexpr int MAX_CONCURRENT = 100;  // Max concurrent traversals

    GPUBatchProcessor<GameType> m_gpu_processor;
    std::mt19937 m_rng;
    std::array<DeepCFRModel, 2>& m_networks;
};

#endif // COROUTINE_TRAVERSAL_HPP