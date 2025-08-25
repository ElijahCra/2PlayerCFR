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
#include <iostream>
#include <torch/torch.h>
#include "Net.hpp"
#include "types.hpp"

static constexpr int MAX_CONCURRENT = 12000;
static constexpr auto POLL_SLEEP = std::chrono::microseconds(100);
static constexpr size_t MIN_BATCH_SIZE = 128;
static constexpr auto MAX_BATCH_WAIT = std::chrono::microseconds(500);

// Forward declarations
template<typename GameType> class TraversalScheduler;

// GPU request with coroutine integration
template<typename GameType>
struct GPURequest {
    std::vector<torch::Tensor> cards;
    torch::Tensor bets;
    torch::Tensor result;                  // Filled when computation complete
    std::atomic<bool> ready{false};        // Use atomic for thread safety
    int player_id = -1;                    // Track which player this is for
};

// Simple awaitable for GPU results
// template<typename GameType>
// class GPUAwaitable {
// public:
//     GPUAwaitable(std::shared_ptr<GPURequest<GameType>> req)
//             : request_weak(req) {} // Store a weak_ptr instead
//
//     bool await_ready() const noexcept {
//         if (auto req = request_weak.lock()) {
//             return req->ready.load();
//         }
//         return true; // If request is gone, we are "ready"
//     }
//
//     bool await_suspend(std::coroutine_handle<> h) noexcept {
//         if (auto req = request_weak.lock()) {
//             req->continuation = h;
//             return !req->ready.load();
//         }
//         return false; // Don't suspend if request is gone
//     }
//
//     torch::Tensor await_resume() noexcept {
//         if (auto req = request_weak.lock()) {
//             return req->result;
//         }
//         return torch::Tensor{}; // Return empty tensor if request expired
//     }
//
// private:
//     std::weak_ptr<GPURequest<GameType>> request_weak;
// };
template<typename GameType>
struct GpuAwaiter {
    TraversalScheduler<GameType> * scheduler;
    std::shared_ptr<GPURequest<GameType>> request;

    bool await_ready() const { return false; } // Always suspend

    void await_suspend(std::coroutine_handle<> h) const {
        scheduler->m_gpu_processor.submit(request, h);
    }

    torch::Tensor await_resume() const {
        return request->result;
    }
};
// GPU Batch Processor - runs on separate thread
template<typename GameType>
class GPUBatchProcessor {
public:
    GPUBatchProcessor(std::array<DeepCFRModel, 2>& networks, torch::Device device)
        : m_networks(networks), m_device(device), m_stop(false)
    {
        for (auto network : m_networks){
            network->to(m_device);
        }
        m_last_batch_time = std::chrono::steady_clock::now();
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

    void submit(std::shared_ptr<GPURequest<GameType>> request, std::coroutine_handle<> handle) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending[request->player_id].push_back({request, handle});
        }
        m_cv.notify_one();
    }

    std::vector<std::pair<std::shared_ptr<GPURequest<GameType>>, std::coroutine_handle<>>> get_ready_requests() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::pair<std::shared_ptr<GPURequest<GameType>>, std::coroutine_handle<>>> ready{};
        ready.swap(m_completed);
        return ready;
    }

private:
    void process_loop() {
        while (!m_stop) {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Wait for work or timeout for batching
            m_cv.wait_for(lock, POLL_SLEEP, [this] {
                return m_stop || has_pending_work();
            });

            // Process batches for each player
            for (int p = 0; p < 2; ++p) {
                if (!m_pending[p].empty() && (m_pending[p].size() >= MIN_BATCH_SIZE || std::chrono::steady_clock::now() - m_last_batch_time > MAX_BATCH_WAIT)) {

                    auto batch = std::move(m_pending[p]);
                    m_pending[p].clear();
                    m_last_batch_time = std::chrono::steady_clock::now();
                    lock.unlock();

                    process_batch(batch, p);

                    lock.lock();
                    m_completed.insert(
                        m_completed.end(),
                        std::make_move_iterator(batch.begin()),
                        std::make_move_iterator(batch.end())
                    );
                    batch.clear();
                }
            }
        }
    }

    bool has_pending_work() {
        return !m_pending[0].empty() || !m_pending[1].empty();
    }

    void process_batch(std::vector<std::pair<std::shared_ptr<GPURequest<GameType>>, std::coroutine_handle<>>>& batch, int player) {
        if (batch.empty()) return;

        size_t batch_size = batch.size();

        std::cout << "batch size: " << batch_size << std::endl;
        int num_card_types = batch[0].first->cards.size();

        // --- 1. Collect tensors from all requests ---
       std::vector<std::vector<torch::Tensor>> card_tensors_by_type(num_card_types);
       std::vector<torch::Tensor> bet_tensors;
       bet_tensors.reserve(batch_size);

        for (const auto& pair : batch) {
           const auto& req = pair.first;
           for (int i = 0; i < num_card_types; ++i) {
               card_tensors_by_type[i].push_back(req->cards[i]);
           }
           bet_tensors.push_back(req->bets);

           // FIX: Clear the tensors in the request to free memory early
           req->cards.clear();
           req->cards.shrink_to_fit();
           req->bets = torch::Tensor{};
        }

        // --- 2. Batch them on the CPU using torch::cat ---
        std::vector<torch::Tensor> batched_cards;
        batched_cards.reserve(num_card_types);
        for (int i = 0; i < num_card_types; ++i) {
            batched_cards.push_back(torch::cat(card_tensors_by_type[i], 0));
            // Clear intermediate vectors to free memory
            card_tensors_by_type[i].clear();
            card_tensors_by_type[i].shrink_to_fit();
        }
        auto batched_bets = torch::cat(bet_tensors, 0);

        // Clear intermediate vectors
        bet_tensors.clear();
        bet_tensors.shrink_to_fit();

        // --- 3. Move the completed batches to GPU in one go ---
        for(auto& t : batched_cards) {
            t = t.to(m_device);
        }
        batched_bets = batched_bets.to(m_device);

        // --- 4. Run inference ---
        torch::Tensor results;
        {
            torch::NoGradGuard no_grad;
            results = m_networks[player]->forward(batched_cards, batched_bets);
        }

        // --- 5. Move results back to CPU at once ---
        auto results_cpu = results.to(torch::kCPU);

        // Free GPU tensors immediately
        for(auto& t : batched_cards) {
            t = torch::Tensor{};
        }
        batched_bets = torch::Tensor{};
        results = torch::Tensor{};

        // --- 6. Distribute results ---
        for (size_t i = 0; i < batch_size; ++i) {
            batch[i].first->result = results_cpu.slice(0, i, i + 1).clone(); // Or the empty_like().copy_() version
            batch[i].first->ready.store(true);
        }

        // Clear the results tensor
        results_cpu = torch::Tensor{};
    }

    std::array<DeepCFRModel, 2>& m_networks;
    torch::Device m_device;

    std::thread m_thread;
    std::atomic<bool> m_stop;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::array<std::vector<std::pair<std::shared_ptr<GPURequest<GameType>>, std::coroutine_handle<>>>, 2> m_pending;
    std::vector<std::pair<std::shared_ptr<GPURequest<GameType>>, std::coroutine_handle<>>> m_completed;
    std::chrono::steady_clock::time_point m_last_batch_time;
};

// Simple coroutine task for traversal
template<typename GameType>
struct TraversalPromise;

template<typename GameType>
class TraversalTask {
public:
    using promise_type = TraversalPromise<GameType>;
    using handle_type = std::coroutine_handle<promise_type>;

    TraversalTask(handle_type h) : m_handle(h) {}

    ~TraversalTask() {
        if (m_handle) {
            m_handle.destroy();
            m_handle = nullptr;
        }
    }

    // Move only
    TraversalTask(TraversalTask&& other) noexcept
        : m_handle(std::exchange(other.m_handle, {})) {}

    TraversalTask& operator=(TraversalTask&& other) noexcept {
        if (this != &other) {
            if (m_handle) {
                m_handle.destroy();
            }
            m_handle = std::exchange(other.m_handle, {});
        }
        return *this;
    }

    // Disable copy
    TraversalTask(const TraversalTask&) = delete;
    TraversalTask& operator=(const TraversalTask&) = delete;

    // Simple awaitable interface for co_await
    bool await_ready() { return m_handle.done(); }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
        m_handle.promise().m_continuation = awaiting;
        return m_handle;  // Symmetric transfer to child task
    }

    float await_resume() {
        return m_handle.promise().m_value;
    }

    void resume() {
        if (m_handle && !m_handle.done())
            m_handle.resume();
    }

    bool done() const {
        return !m_handle || m_handle.done();
    }

    float get_result() {
        if (!done()) {
            m_handle.resume();
        }
        return m_handle.promise().m_value;
    }

private:
    handle_type m_handle;
};

template<typename GameType>
struct TraversalPromise {
    float m_value = 0.0f;
    std::coroutine_handle<> m_continuation;

    TraversalTask<GameType> get_return_object() {
        return TraversalTask<GameType>{
            std::coroutine_handle<TraversalPromise>::from_promise(*this)
        };
    }

    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept {
        struct final_awaiter {
            std::coroutine_handle<> m_cont;
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
                if (m_cont)
                    return m_cont;  // Symmetric transfer to parent
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };
        return final_awaiter{m_continuation};
    }

    void return_value(float v) { m_value = v; }
    void unhandled_exception() { std::terminate(); }
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

    // Simplified coroutine traversal
    TraversalTask<GameType> traverse_cfr_coro(GameType game,
                                              int update_player,
                                              int iteration,
                                              float prob_update_player) {
        // Terminal node
        if (game.getType() == "terminal") {
            ++m_nodes_touched;
            float utility = game.getUtility(update_player);
            co_return utility;
        }

        // Chance node
        if (game.getType() == "chance") {
            ++m_nodes_touched;
            game.transition(GameType::Action::Chance);
            float result = co_await traverse_cfr_coro(game, update_player, iteration, prob_update_player);
            co_return result;
        }

        ++m_nodes_touched;
        ++m_gpu_nodes_touched;

        // Decision node - prepare and submit GPU request
        int current_player = game.getCurrentPlayer();
        auto legal_actions = game.getActions();

        auto gpu_request = std::make_shared<GPURequest<GameType>>();
        gpu_request->cards = game.getCardTensors(current_player, game.getCurrentRound());
        gpu_request->bets = game.getBetTensor();
        gpu_request->player_id = current_player;

        torch::Tensor advantages_tensor = co_await GpuAwaiter{this, gpu_request};
        // Extract legal advantages and compute strategy
        std::vector<float> legal_advantages;
        std::vector<int> legal_indices;
        legal_advantages.reserve(legal_actions.size());
        legal_indices.reserve(legal_actions.size());

        for (auto action : legal_actions) {
            int idx = GameType::ActionMapping::getActionIndex(action);
            legal_indices.push_back(idx);
            legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
        }

        auto strategy = compute_strategy_from_advantages(legal_advantages);
        float node_value = 0.0f;

        if (current_player == update_player) {
            // Traverser: explore all actions
            for (size_t a = 0; a < legal_actions.size(); ++a) {
                GameType next_game(game);
                next_game.transition(legal_actions[a]);

                float cf_value = co_await traverse_cfr_coro(
                    next_game, update_player, iteration,
                    prob_update_player * strategy[a]
                );
                node_value += strategy[a] * cf_value;
            }
        } else {
            // Opponent: sample single action
            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx = dist(m_rng);

            GameType next_game(game);
            next_game.transition(legal_actions[action_idx]);

            node_value = co_await traverse_cfr_coro(
                next_game, update_player, iteration, prob_update_player
            );
        }

        co_return node_value;
    }

    // Main training loop with work-stealing
    void train(uint32_t iterations, int traversals_per_iteration) {
        for (uint32_t iter = 1; iter <= iterations; ++iter) {
            std::cout << "Iteration " << iter << "/" << iterations << std::endl;

            for (int p = 0; p < GameType::PlayerNum; ++p) {
                run_traversals_for_player(p, iter, traversals_per_iteration);
            }
        }
    }
    GPUBatchProcessor<GameType> m_gpu_processor;

private:
    void run_traversals_for_player(int player, int iteration, int num_traversals) {
        std::deque<std::unique_ptr<TraversalTask<GameType>>> active_tasks;
        uint32_t nodes_begin = m_nodes_touched;
        uint32_t gpu_nodes_begin = m_gpu_nodes_touched;
        int completed = 0;
        int started = 0;

        auto start_time = std::chrono::high_resolution_clock::now();

        while (completed < num_traversals) {
            // Process GPU completions
            auto ready_requests = m_gpu_processor.get_ready_requests();
            for (auto& [req, handle] : ready_requests) { // Use structured binding
                if (handle && !handle.done()) {
                    handle.resume();
                }
            }

            // Check for completed tasks
            auto it = active_tasks.begin();
            while (it != active_tasks.end()) {
                if ((*it)->done()) {
                    completed++;
                    it = active_tasks.erase(it);
                } else {
                    ++it;
                }
            }

            // Start new traversals if capacity available
            while (started < num_traversals && active_tasks.size() < MAX_CONCURRENT) {
                if (started % 2000 == 0) std::cout << started << std::endl;
                GameType game(m_rng);
                auto task = std::make_unique<TraversalTask<GameType>>(
                    traverse_cfr_coro(game, player, iteration, 1.0f)
                );

                task->resume();  // Start the coroutine

                if (!task->done()) {
                    active_tasks.push_back(std::move(task));
                } else {
                    completed++;
                }
                started++;
            }

            // Yield if no work available
            if (ready_requests.empty() && active_tasks.size() == MAX_CONCURRENT) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time
        );
        std::cout << "Player " << player << " completed " << num_traversals
                  << " traversals in " << duration
                  << " GPU Nodes/ms: " << (m_gpu_nodes_touched - gpu_nodes_begin) / duration.count()
                  << std::endl;
    }

    std::vector<float> compute_strategy_from_advantages(const std::vector<float>& advantages) {
        std::vector<float> strategy(advantages.size());
        float sum_positive = 0.0f;

        for (size_t i = 0; i < advantages.size(); ++i) {
            float positive = std::max(0.0f, advantages[i]);
            strategy[i] = positive;
            sum_positive += positive;
        }

        if (sum_positive > 0) {
            for (auto& s : strategy) {
                s /= sum_positive;
            }
        } else {
            float uniform = 1.0f / advantages.size();
            std::fill(strategy.begin(), strategy.end(), uniform);
        }

        return strategy;
    }




    std::mt19937 m_rng;
    std::array<DeepCFRModel, 2>& m_networks;
    uint32_t m_nodes_touched{};
    uint32_t m_gpu_nodes_touched{};
};

#endif // COROUTINE_TRAVERSAL_HPP