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

#include "HybridAdvantageStorage.hpp"
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
    std::coroutine_handle<> continuation;  // Coroutine to resume when ready
    torch::Tensor result;                  // Filled when computation complete
    std::atomic<bool> ready{false};        // Use atomic for thread safety
    int player_id = -1;                    // Track which player this is for
};

// Simple awaitable for GPU results
template<typename GameType>
class GPUAwaitable {
public:
    GPUAwaitable(std::shared_ptr<GPURequest<GameType>> req)
        : request(req) {}

    bool await_ready() const noexcept {
        return request->ready.load();
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept {
        request->continuation = h;
        // Return false to resume immediately if ready, true to suspend
        return !request->ready.load();
    }

    torch::Tensor await_resume() noexcept {
        request->continuation = nullptr;
        return request->result;
    }

private:
    std::shared_ptr<GPURequest<GameType>> request;
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

        // --- 1. Collect tensors from all requests ---
        std::vector<std::vector<torch::Tensor>> card_tensors_by_type(num_card_types);
        std::vector<torch::Tensor> bet_tensors;
        bet_tensors.reserve(batch_size);

        for (const auto& req : batch) {
            for (int i = 0; i < num_card_types; ++i) {
                card_tensors_by_type[i].push_back(req->cards[i]);
            }
            bet_tensors.push_back(req->bets);
        }

        // --- 2. Batch them on the CPU using torch::cat ---
        std::vector<torch::Tensor> batched_cards;
        for (int i = 0; i < num_card_types; ++i) {
            batched_cards.push_back(torch::cat(card_tensors_by_type[i], 0));
        }
        auto batched_bets = torch::cat(bet_tensors, 0);

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

        // --- 6. Distribute results ---
        for (size_t i = 0; i < batch_size; ++i) {
            batch[i]->result = results_cpu.slice(0, i, i + 1).clone();
            batch[i]->ready.store(true);
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
        if (m_handle)
            m_handle.destroy();
    }

    // Move only
    TraversalTask(TraversalTask&& other) noexcept
        : m_handle(std::exchange(other.m_handle, {})) {}

    TraversalTask& operator=(TraversalTask&& other) noexcept {
        if (this != &other) {
            if (m_handle)
                m_handle.destroy();
            m_handle = std::exchange(other.m_handle, {});
        }
        return *this;
    }

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
          m_networks(networks),
          m_storage_config(createStorageConfig()){

        m_gpu_processor.start();
        for (int i = 0; i < 2; ++i) {
            m_hybrid_storage[i] = std::make_unique<HybridAdvantageStorage<GameType>>(m_storage_config);
        }

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

        m_gpu_processor.submit(gpu_request, current_player);

        // Suspend until GPU result is ready
        torch::Tensor advantages_tensor = co_await GPUAwaitable<GameType>(gpu_request);

        // Extract legal advantages and compute strategy
        std::vector<float> legal_advantages;
        std::vector<int> legal_indices;
        for (auto action : legal_actions) {
            int idx = GameType::ActionMapping::getActionIndex(action);
            legal_indices.push_back(idx);
            legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
        }

        auto strategy = compute_strategy_from_advantages(legal_advantages);
        float node_value = 0.0f;

        if (current_player == update_player) {
            // Traverser: explore all actions
            std::vector<float> counterfactual_values(legal_actions.size());

            for (size_t a = 0; a < legal_actions.size(); ++a) {
                GameType next_game(game);
                next_game.transition(legal_actions[a]);

                counterfactual_values[a] = co_await enhanced_traverse_cfr_coro(
                    next_game, update_player, iteration,
                    prob_update_player * strategy[a]
                );
                node_value += strategy[a] * counterfactual_values[a];
            }

            // Compute advantages (instantaneous regrets)
            std::vector<float> instant_regrets(legal_actions.size());
            for (size_t a = 0; a < legal_actions.size(); ++a) {
                instant_regrets[a] = counterfactual_values[a] - node_value;
            }

            // Store in hybrid storage (thread-safe)
            TrainingSampleAdvantage sample;
            sample.infoset = {gpu_request->cards, gpu_request->bets};
            sample.iteration = iteration;
            sample.legal_action_indices = legal_indices;
            sample.advantages = instant_regrets;
            sample.weight = static_cast<float>(iteration);

            // Thread-safe add to storage
            std::mt19937 local_rng(std::random_device{}());
            m_hybrid_storage[update_player]->add_sample(sample, local_rng);

        } else {
            // Opponent: sample single action
            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx = dist(this->m_rng);

            GameType next_game(game);
            next_game.transition(legal_actions[action_idx]);

            node_value = co_await enhanced_traverse_cfr_coro(
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

                train_from_hybrid_storage(p);

                // Periodic flush to disk
                if (iter % 10 == 0) {
                    m_hybrid_storage[p]->force_flush();
                }

                // Log storage statistics
                std::cout << "Player " << p << " storage - Memory: "
                         << m_hybrid_storage[p]->in_memory_size()
                         << ", Disk: " << m_hybrid_storage[p]->disk_size()
                         << std::endl;
            }
        }


        // Final flush
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            m_hybrid_storage[p]->force_flush();
        }
    }

    HybridAdvantageStorage<GameType>* get_storage(int player) {
        return m_hybrid_storage[player].get();
    }

private:
    void train_from_hybrid_storage(int player) {
        auto* storage = m_enhanced_scheduler.get_storage(player);

        if (storage->in_memory_size() < this->BATCH_SIZE) {
            std::cout << "Player " << player << " training skipped: insufficient samples" << std::endl;
            return;
        }

        // Reinitialize network
        this->m_advantage_networks[player] = DeepCFRModel(
            GameType::NUM_CARD_TYPES,
            GameType::NUM_BET_FEATURES,
            GameType::MAX_ACTIONS
        );
        advantage_networks[player]->to(this->device);
        this->m_advantage_optimizers[player] = torch::optim::Adam(
            this->m_advantage_networks[player]->parameters(),
            this->LEARNING_RATE
        );

        this->m_advantage_networks[player]->train();

        // Training loop using batches from storage
        const int num_epochs = 5;
        std::mt19937 local_rng(std::random_device{}());

        for (int epoch = 0; epoch < num_epochs; ++epoch) {
            for (int batch_idx = 0; batch_idx < this->SGD_ITERATIONS; ++batch_idx) {
                // Get batch from hybrid storage
                auto batch = storage->get_batch(this->BATCH_SIZE, local_rng);

                if (batch.batch_size == 0) break;

                // Move to device
                for (auto& card_tensor : batch.cards) {
                    card_tensor = card_tensor.to(this->m_device);
                }
                batch.bets = batch.bets.to(this->m_device);
                batch.targets = batch.targets.to(this->m_device);
                batch.masks = batch.masks.to(this->m_device);
                batch.weights = batch.weights.to(this->m_device);

                // Forward pass
                auto predictions = this->m_advantage_networks[player]->forward(
                    batch.cards, batch.bets
                );

                // Compute loss
                auto squared_error = (predictions - batch.targets).pow(2);
                auto weighted_error = squared_error * batch.masks * batch.weights;
                auto loss = weighted_error.sum() / ((batch.masks * batch.weights).sum() + 1e-9);

                // Backward pass
                m_advantage_optimizers[player].zero_grad();
                loss.backward();
                this->m_advantage_networks[player]->clip_gradients(this->GRADIENT_CLIP_NORM);
                this->m_advantage_optimizers[player].step();

                if (batch_idx % 100 == 0) {
                    std::cout << "Player " << player << " Epoch " << epoch
                             << " Batch " << batch_idx
                             << " Loss: " << loss.item<float>() << std::endl;
                }
            }
        }
    }

    void run_traversals_for_player(int player, int iteration, int num_traversals) {
        std::deque<std::unique_ptr<TraversalTask<GameType>>> active_tasks;
        int completed = 0;
        int started = 0;

        // Thread pool for parallel traversals
        const int max_concurrent = std::min(MAX_CONCURRENT, num_traversals);

        auto start_time = std::chrono::high_resolution_clock::now();

        while (completed < num_traversals) {
            // Process GPU completions
            auto ready_requests = this->m_gpu_processor.get_ready_requests();
            for (auto& req : ready_requests) {
                if (req->continuation && !req->continuation.done()) {
                    req->continuation.resume();
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

            // Start new traversals
            while (started < num_traversals && active_tasks.size() < max_concurrent) {
                GameType game(this->m_rng);
                auto task = std::make_unique<TraversalTask<GameType>>(
                    enhanced_traverse_cfr_coro(game, player, iteration, 1.0f)
                );

                task->resume();

                if (!task->done()) {
                    active_tasks.push_back(std::move(task));
                } else {
                    completed++;
                }
                started++;
            }

            // Brief yield if no work
            if (ready_requests.empty() && active_tasks.size() == max_concurrent) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time
        );

        std::cout << "Player " << player << " completed " << num_traversals
                  << " traversals in " << duration.count() << "ms" << std::endl;
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
            std::ranges::fill(strategy, uniform);
        }

        return strategy;
    }


    static HybridAdvantageStorage<GameType>::Config createStorageConfig() {
        typename HybridAdvantageStorage<GameType>::Config config;
        config.in_memory_capacity = 2000000;  // 2M in memory
        config.flush_batch_size = 50000;      // Flush every 50k samples
        config.max_db_size = 100000000;       // 100M max on disk
        config.flush_interval = std::chrono::milliseconds(10000);  // Flush every 10s
        config.num_flush_threads = 4;         // 4 background flush threads
        config.db_path = "./deep_cfr_advantage_db";
        return config;
    }

    HybridAdvantageStorage<GameType>::Config m_storage_config;
    std::array<std::unique_ptr<HybridAdvantageStorage<GameType>>, 2> m_hybrid_storage;

    GPUBatchProcessor<GameType> m_gpu_processor;
    std::mt19937 m_rng;
    std::array<DeepCFRModel, 2>& m_networks;
    uint32_t m_nodes_touched{};
    uint32_t m_gpu_nodes_touched{};
};

#endif // COROUTINE_TRAVERSAL_HPP