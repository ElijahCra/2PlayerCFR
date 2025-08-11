// CoRoutineDeepCFR.hpp
#ifndef COROUTINE_DEEPCFR_HPP
#define COROUTINE_DEEPCFR_HPP

#include <coroutine>
#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <future>
#include <thread>
#include <vector>
#include <chrono>
#include <functional>
#include <random>
#include <numeric>
#include <algorithm>

#include "torch/torch.h"
#include "types.hpp"
#include "Net.hpp"
#include "AdvantageMemoryBuffer.hpp"
#include "DataLoader.hpp"

// Forward declarations
template<typename GameType> class InferenceBatcher;
template<typename GameType> class TaskScheduler;
template<typename GameType> struct TraversalTask;

// Inference request that gets batched
template<typename GameType>
struct InferenceRequest {
    size_t request_id;
    int player;
    std::vector<torch::Tensor> cards;
    torch::Tensor bets;
    std::promise<torch::Tensor> result_promise;
};

// Awaitable for inference results
template<typename GameType>
struct InferenceAwaitable {
    InferenceBatcher<GameType>* batcher;
    size_t request_id;
    std::future<torch::Tensor> result_future;

    bool await_ready() const noexcept {
        return result_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    void await_suspend(std::coroutine_handle<> h) {
        // Register this coroutine to be resumed when result is ready
        batcher->register_continuation(request_id, h);
    }

    torch::Tensor await_resume() {
        return result_future.get();
    }
};

// The inference batcher - accumulates requests and processes in batches
template<typename GameType>
class InferenceBatcher {
public:
    InferenceBatcher(std::array<DeepCFRModel, 2>& networks,
                     torch::Device device,
                     size_t batch_size = 256,
                     std::chrono::microseconds batch_timeout = std::chrono::microseconds(100))
        : m_networks(networks),
          m_device(device),
          m_batch_size(batch_size),
          m_batch_timeout(batch_timeout),
          m_running(true) {

        // Start the batch processing thread
        m_batch_processor = std::thread(&InferenceBatcher::process_batches, this);
    }

    ~InferenceBatcher() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_running = false;
        }
        m_cv.notify_all();
        if (m_batch_processor.joinable()) {
            m_batch_processor.join();
        }
    }

    // Submit an inference request and get an awaitable
    InferenceAwaitable<GameType> submit_inference(int player,
                                                  const std::vector<torch::Tensor>& cards,
                                                  const torch::Tensor& bets) {
        auto request = std::make_shared<InferenceRequest<GameType>>();
        request->request_id = m_next_request_id.fetch_add(1);
        request->player = player;
        request->cards = cards;
        request->bets = bets;

        auto future = request->result_promise.get_future();

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pending_requests[player].push_back(request);

            // Check if we should trigger batch processing
            if (m_pending_requests[player].size() >= m_batch_size) {
                m_cv.notify_one();
            }
        }

        // Set up timeout-based batch processing
        schedule_batch_timeout();

        return InferenceAwaitable<GameType>{this, request->request_id, std::move(future)};
    }

    void register_continuation(size_t request_id, std::coroutine_handle<> handle) {
        std::unique_lock<std::mutex> lock(m_continuation_mutex);
        m_continuations[request_id] = handle;
    }

private:
    void process_batches() {
        while (m_running) {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Wait for batch to be ready or timeout
            m_cv.wait_for(lock, m_batch_timeout, [this] {
                if (!m_running) return true;
                for (int p = 0; p < 2; ++p) {
                    if (m_pending_requests[p].size() >= m_batch_size ||
                        (m_pending_requests[p].size() > 0 && should_process_batch(p))) {
                        return true;
                    }
                }
                return false;
            });

            if (!m_running) break;

            // Process batches for each player
            for (int player = 0; player < 2; ++player) {
                if (m_pending_requests[player].empty()) continue;

                // Determine batch size
                size_t batch_size = std::min(m_batch_size, m_pending_requests[player].size());
                if (batch_size == 0) continue;

                // Extract batch
                std::vector<std::shared_ptr<InferenceRequest<GameType>>> batch;
                batch.reserve(batch_size);
                for (size_t i = 0; i < batch_size; ++i) {
                    batch.push_back(m_pending_requests[player][i]);
                }
                m_pending_requests[player].erase(
                    m_pending_requests[player].begin(),
                    m_pending_requests[player].begin() + batch_size
                );

                // Unlock while processing
                lock.unlock();

                // Process batch on GPU
                process_single_batch(player, batch);

                lock.lock();
            }
        }
    }

    void process_single_batch(int player,
                             const std::vector<std::shared_ptr<InferenceRequest<GameType>>>& batch) {
        try {
            // Prepare batched tensors
            std::vector<std::vector<torch::Tensor>> all_cards(GameType::NUM_CARD_TYPES);
            std::vector<torch::Tensor> all_bets;

            for (const auto& request : batch) {
                for (size_t i = 0; i < request->cards.size(); ++i) {
                    if (i >= all_cards.size()) all_cards.resize(i + 1);
                    all_cards[i].push_back(request->cards[i]);
                }
                all_bets.push_back(request->bets);
            }

            // Stack tensors for batch processing
            std::vector<torch::Tensor> batched_cards;
            for (auto& card_list : all_cards) {
                if (!card_list.empty()) {
                    batched_cards.push_back(torch::stack(card_list).squeeze(1).to(m_device));
                }
            }
            auto batched_bets = torch::stack(all_bets).squeeze(1).to(m_device);

            // Run inference
            torch::NoGradGuard no_grad;
            auto results = m_networks[player]->forward(batched_cards, batched_bets);

            // Distribute results
            for (size_t i = 0; i < batch.size(); ++i) {
                auto result = results[i].unsqueeze(0).to(torch::kCPU);
                batch[i]->result_promise.set_value(result);

                // Resume coroutine if it's waiting
                {
                    std::unique_lock<std::mutex> lock(m_continuation_mutex);
                    auto it = m_continuations.find(batch[i]->request_id);
                    if (it != m_continuations.end()) {
                        auto handle = it->second;
                        m_continuations.erase(it);
                        lock.unlock();
                        handle.resume();
                    }
                }
            }
        } catch (const std::exception& e) {
            // Set exception for all requests in batch
            for (const auto& request : batch) {
                request->result_promise.set_exception(std::current_exception());
            }
        }
    }

    bool should_process_batch(int player) {
        auto now = std::chrono::steady_clock::now();
        auto time_since_last = now - m_last_batch_time[player];
        return time_since_last >= m_batch_timeout;
    }

    void schedule_batch_timeout() {
        // This could be implemented with a timer, but for simplicity we rely on wait_for
    }

    std::array<DeepCFRModel, 2>& m_networks;
    torch::Device m_device;
    size_t m_batch_size;
    std::chrono::microseconds m_batch_timeout;

    std::atomic<bool> m_running;
    std::atomic<size_t> m_next_request_id{0};

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::array<std::vector<std::shared_ptr<InferenceRequest<GameType>>>, 2> m_pending_requests;
    std::array<std::chrono::steady_clock::time_point, 2> m_last_batch_time;

    std::mutex m_continuation_mutex;
    std::unordered_map<size_t, std::coroutine_handle<>> m_continuations;

    std::thread m_batch_processor;
};

// Traversal state for coroutine
template<typename GameType>
struct TraversalState {
    std::unique_ptr<GameType> game;  // Use unique_ptr to allow default construction
    int update_player = 0;
    int current_iter = 0;
    float prob_update_player = 1.0f;
    float result = 0.0f;

    // For collecting samples
    std::array<std::vector<TrainingSampleAdvantage>, 2>* adv_samples = nullptr;
    std::vector<TrainingSampleStrategy>* strat_samples = nullptr;
};

// The coroutine promise type
template<typename GameType>
struct TraversalPromise {
    TraversalState<GameType> state;
    std::exception_ptr exception;

    // Default constructor
    TraversalPromise() = default;

    TraversalTask<GameType> get_return_object();
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    void return_value(float value) {
        state.result = value;
    }

    void unhandled_exception() {
        exception = std::current_exception();
    }
};

// The coroutine type
template<typename GameType>
struct TraversalTask {
    using promise_type = TraversalPromise<GameType>;

    std::coroutine_handle<promise_type> h;

    TraversalTask(std::coroutine_handle<promise_type> handle) : h(handle) {}

    ~TraversalTask() {
        if (h) h.destroy();
    }

    // Move only
    TraversalTask(TraversalTask&& other) noexcept : h(std::exchange(other.h, {})) {}
    TraversalTask& operator=(TraversalTask&& other) noexcept {
        if (this != &other) {
            if (h) h.destroy();
            h = std::exchange(other.h, {});
        }
        return *this;
    }

    float get_result() {
        if (!h.done()) {
            h.resume();
        }
        if (h.promise().exception) {
            std::rethrow_exception(h.promise().exception);
        }
        return h.promise().state.result;
    }

    bool done() const { return h.done(); }
    void resume() { if (!h.done()) h.resume(); }
};

template<typename GameType>
TraversalTask<GameType> TraversalPromise<GameType>::get_return_object() {
    return TraversalTask<GameType>{std::coroutine_handle<TraversalPromise>::from_promise(*this)};
}

// Task scheduler for managing coroutines
template<typename GameType>
class TaskScheduler {
public:
    TaskScheduler(size_t num_threads = std::thread::hardware_concurrency())
        : m_num_threads(num_threads), m_running(true) {

        for (size_t i = 0; i < num_threads; ++i) {
            m_workers.emplace_back(&TaskScheduler::worker_thread, this);
        }
    }

    ~TaskScheduler() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_running = false;
        }
        m_cv.notify_all();

        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void schedule(std::shared_ptr<TraversalTask<GameType>> task) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_ready_tasks.push(task);
        }
        m_cv.notify_one();
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_done_cv.wait(lock, [this] {
            return m_ready_tasks.empty() && m_active_tasks == 0;
        });
    }

private:
    void worker_thread() {
        while (m_running) {
            std::shared_ptr<TraversalTask<GameType>> task;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] {
                    return !m_running || !m_ready_tasks.empty();
                });

                if (!m_running) break;

                if (!m_ready_tasks.empty()) {
                    task = m_ready_tasks.front();
                    m_ready_tasks.pop();
                    m_active_tasks++;
                }
            }

            if (task && !task->done()) {
                task->resume();

                // If not done, reschedule
                if (!task->done()) {
                    schedule(task);
                }

                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_active_tasks--;
                }
                m_done_cv.notify_one();
            }
        }
    }

    size_t m_num_threads;
    std::atomic<bool> m_running;
    std::vector<std::thread> m_workers;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_done_cv;
    std::queue<std::shared_ptr<TraversalTask<GameType>>> m_ready_tasks;
    std::atomic<size_t> m_active_tasks{0};
};

// Modified DeepRegretMinimizer with coroutine support
template<typename GameType>
class CoRoutineDeepRegretMinimizer {
public:
    CoRoutineDeepRegretMinimizer(uint32_t seed = std::random_device()())
        : m_rng(seed),
          m_device(torch::cuda::is_available() ? torch::kCUDA : torch::mps::is_available() ? torch::kMPS : torch::kCPU),
          m_strategy_network(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES, GameType::MAX_ACTIONS),
          m_strategy_optimizer({m_strategy_network->parameters()}, torch::optim::AdamOptions(LEARNING_RATE)),
          m_adv_memories{AdvantageMemoryBuffer<GameType>(MEMORY_SIZE), AdvantageMemoryBuffer<GameType>(MEMORY_SIZE)} {

        // Initialize networks
        m_strategy_network->to(m_device);
        for (int i = 0; i < 2; ++i) {
            m_advantage_networks[i] = DeepCFRModel(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES,
                                                   GameType::MAX_ACTIONS);
            m_advantage_networks[i]->to(m_device);
            m_advantage_optimizers.emplace_back(m_advantage_networks[i]->parameters(), LEARNING_RATE);
        }
        m_strategy_memory.reserve(MEMORY_SIZE);
    }

    void TrainCoroutine(uint32_t iterations, size_t num_threads = std::thread::hardware_concurrency()) {
        // Create batcher and scheduler
        InferenceBatcher<GameType> batcher(m_advantage_networks, m_device, 256);
        TaskScheduler<GameType> scheduler(num_threads);

        for (uint32_t iter = 1; iter <= iterations; ++iter) {
            std::cout << "Iteration " << iter << "/" << iterations << std::endl;

            for (int p = 0; p < GameType::PlayerNum; ++p) {
                auto t1 = std::chrono::high_resolution_clock::now();

                // Create traversal tasks
                std::vector<std::shared_ptr<TraversalTask<GameType>>> tasks;
                std::array<std::vector<TrainingSampleAdvantage>, 2> all_adv_samples;
                std::vector<TrainingSampleStrategy> all_strat_samples;
                std::mutex samples_mutex;

                for (int k = 0; k < K_TRAVERSALS; ++k) {
                    // Create local storage for this traversal
                    auto local_adv = std::make_shared<std::array<std::vector<TrainingSampleAdvantage>, 2>>();
                    auto local_strat = std::make_shared<std::vector<TrainingSampleStrategy>>();

                    // Create and schedule coroutine task
                    auto task = std::make_shared<TraversalTask<GameType>>(
                        traverse_cfr_coroutine(batcher, p, iter, 1.0f,
                                             local_adv.get(), local_strat.get(), samples_mutex,
                                             all_adv_samples, all_strat_samples)
                    );
                    tasks.push_back(task);
                    scheduler.schedule(task);
                }

                // Wait for all traversals to complete
                scheduler.wait_all();

                // Collect results and add to memory
                for (int player = 0; player < 2; ++player) {
                    for (const auto& sample : all_adv_samples[player]) {
                        m_adv_memories[p].add_sample(sample, m_rng);
                    }
                }
                for (const auto& sample : all_strat_samples) {
                    add_to_strategy_memory(m_strategy_memory, sample, MEMORY_SIZE);
                }

                auto t2 = std::chrono::high_resolution_clock::now();
                auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
                std::cout << "Training for player: " << p << " time: " << ms_int.count() << "ms" << std::endl;

                // Train advantage network
                train_advantage_network(p);
            }
        }

        // Final strategy network training
        train_strategy_network();
    }

private:
    // Coroutine version of traverse_cfr
    TraversalTask<GameType> traverse_cfr_coroutine(
        InferenceBatcher<GameType>& batcher,
        int update_player,
        int current_iter,
        float prob_update_player,
        std::array<std::vector<TrainingSampleAdvantage>, 2>* local_adv,
        std::vector<TrainingSampleStrategy>* local_strat,
        std::mutex& samples_mutex,
        std::array<std::vector<TrainingSampleAdvantage>, 2>& all_adv_samples,
        std::vector<TrainingSampleStrategy>& all_strat_samples) {

        // Create game instance with RNG
        GameType game(m_rng);
        return traverse_cfr_coroutine_impl(
            std::move(game), batcher, update_player, current_iter, prob_update_player,
            local_adv, local_strat, samples_mutex, all_adv_samples, all_strat_samples
        );
    }

    TraversalTask<GameType> traverse_cfr_coroutine_impl(
        GameType&& game,
        InferenceBatcher<GameType>& batcher,
        int update_player,
        int current_iter,
        float prob_update_player,
        std::array<std::vector<TrainingSampleAdvantage>, 2>* local_adv,
        std::vector<TrainingSampleStrategy>* local_strat,
        std::mutex& samples_mutex,
        std::array<std::vector<TrainingSampleAdvantage>, 2>& all_adv_samples,
        std::vector<TrainingSampleStrategy>& all_strat_samples) {

        // Terminal node
        if (game.getType() == "terminal") {
            co_return game.getUtility(update_player);
        }

        // Chance node
        if (game.getType() == "chance") {
            game.transition(GameType::Action::Chance);
            co_return co_await traverse_cfr_coroutine_impl(
                std::move(game), batcher, update_player, current_iter, prob_update_player,
                local_adv, local_strat, samples_mutex, all_adv_samples, all_strat_samples
            );
        }

        int current_player = game.getCurrentPlayer();
        auto legal_actions = game.getActions();
        std::vector<int> legal_indices;
        for (auto action : legal_actions) {
            legal_indices.push_back(GameType::ActionMapping::getActionIndex(action));
        }

        // Get tensors
        auto cards_cpu = game.getCardTensors(game.getCurrentPlayer(), game.getCurrentRound());
        auto bets_cpu = game.getBetTensor();

        // Submit inference request and await result
        auto advantages_tensor = co_await batcher.submit_inference(current_player, cards_cpu, bets_cpu);

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

            // Launch all child traversals in parallel
            std::vector<std::future<float>> futures;
            for (size_t a = 0; a < legal_actions.size(); ++a) {
                GameType next_game(game);
                next_game.transition(legal_actions[a]);

                futures.push_back(std::async(std::launch::async, [this, next_game = std::move(next_game),
                                                                  &batcher, update_player, current_iter,
                                                                  prob_update_player, &strategy, a,
                                                                  local_adv, local_strat, &samples_mutex,
                                                                  &all_adv_samples, &all_strat_samples]() mutable {
                    auto task = traverse_cfr_coroutine_impl(
                        std::move(next_game), batcher, update_player, current_iter,
                        prob_update_player * strategy[a],
                        local_adv, local_strat, samples_mutex, all_adv_samples, all_strat_samples
                    );
                    return task.get_result();
                }));
            }

            // Collect results
            for (size_t a = 0; a < legal_actions.size(); ++a) {
                counterfactual_values[a] = futures[a].get();
                node_value += strategy[a] * counterfactual_values[a];
            }

            // Compute advantages
            std::vector<float> instant_regrets(legal_actions.size());
            for (size_t a = 0; a < legal_actions.size(); ++a) {
                instant_regrets[a] = counterfactual_values[a] - node_value;
            }

            // Store sample
            TrainingSampleAdvantage sample;
            sample.infoset = {cards_cpu, bets_cpu};
            sample.iteration = current_iter;
            sample.legal_action_indices = legal_indices;
            sample.advantages = instant_regrets;
            sample.weight = static_cast<float>(current_iter);

            {
                std::lock_guard<std::mutex> lock(samples_mutex);
                all_adv_samples[update_player].push_back(sample);
            }

            co_return node_value;

        } else {
            // Opponent: sample single action
            TrainingSampleStrategy sample;
            sample.infoset = {cards_cpu, bets_cpu};
            sample.iteration = current_iter;
            sample.legal_action_indices = legal_indices;
            sample.strategy = strategy;
            sample.weight = static_cast<float>(current_iter);

            {
                std::lock_guard<std::mutex> lock(samples_mutex);
                all_strat_samples.push_back(sample);
            }

            // Sample action
            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx = dist(m_rng);
            game.transition(legal_actions[action_idx]);

            co_return co_await traverse_cfr_coroutine_impl(
                std::move(game), batcher, update_player, current_iter, prob_update_player,
                local_adv, local_strat, samples_mutex, all_adv_samples, all_strat_samples
            );
        }
    }

    // Helper functions (same as original)
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

    template<typename T>
    void add_to_strategy_memory(std::vector<T>& memory, const T& sample, size_t max_size) {
        if (memory.size() < max_size) {
            memory.push_back(sample);
        } else {
            std::uniform_int_distribution<size_t> dist(0, memory.size());
            size_t idx = dist(m_rng);
            if (idx < max_size) {
                memory[idx] = sample;
            }
        }
    }

    void train_advantage_network(int player) {
        // Don't train if the memory buffer is smaller than one batch.
        if (m_adv_memories[player].size() < BATCH_SIZE) {
            std::cout << "Player " << player << " advantage network training skipped: not enough samples." << std::endl;
            return;
        }

        auto t1 = std::chrono::high_resolution_clock::now();

        // Reinitialize network and optimizer to train from scratch.
        m_advantage_networks[player] = DeepCFRModel(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES, GameType::MAX_ACTIONS);
        m_advantage_networks[player]->to(m_device);
        m_advantage_optimizers[player] = torch::optim::Adam(m_advantage_networks[player]->parameters(), LEARNING_RATE);
        m_advantage_networks[player]->train();

        // Determine how many samples and batches to train on.
        size_t total_samples_to_train = std::min(BATCH_SIZE * SGD_ITERATIONS, m_adv_memories[player].size());
        int total_batches = total_samples_to_train / BATCH_SIZE;

        // Instantiate and start the asynchronous data loader.
        DataLoader<GameType> data_loader(m_adv_memories[player], BATCH_SIZE, total_batches, m_device, m_rng);
        data_loader.start();

        float total_loss = 0.0f;

        // The main training loop.
        for (int i = 0; i < total_batches; ++i) {
            // Get a pre-fetched, pre-processed batch from the worker thread.
            auto batch = data_loader.get_batch();
            if (!batch.is_valid) {
                break; // End of the data stream from the loader.
            }

            // Forward and Backward Pass
            auto predictions = m_advantage_networks[player]->forward(batch.cards, batch.bets);
            auto squared_error = (predictions - batch.targets).pow(2);
            auto weighted_error = squared_error * batch.masks * batch.weights;
            auto loss = weighted_error.sum() / ((batch.masks * batch.weights).sum() + 1e-9);

            m_advantage_optimizers[player].zero_grad();
            loss.backward();
            m_advantage_networks[player]->clip_gradients(GRADIENT_CLIP_NORM);
            m_advantage_optimizers[player].step();
            total_loss += loss.template item<float>();
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
        long long ms_count = ms_int.count();
        if (ms_count == 0) ms_count = 1;

        std::cout << "Player " << player << " advantage network training for " << total_samples_to_train
                  << " samples, total loss: " << total_loss
                  << " loss/sample " << total_loss / total_samples_to_train
                  << " samples/ms: " << total_samples_to_train / ms_count
                  << " iter runtime " << ms_int.count() << "ms" << std::endl;
    }

    void train_strategy_network() {
        if (m_strategy_memory.empty()) return;

        m_strategy_network->train();

        // Prep batches
        size_t total_samples = std::min(BATCH_SIZE * SGD_ITERATIONS, m_strategy_memory.size());
        std::vector<std::vector<torch::Tensor>> all_cards_cpu(total_samples);
        std::vector<torch::Tensor> all_bets_cpu(total_samples);
        std::vector<std::vector<float>> all_targets_cpu(total_samples);
        std::vector<std::vector<float>> all_masks_cpu(total_samples);
        std::vector<float> all_weights_cpu(total_samples);

        std::vector<size_t> all_indices(total_samples);
        std::iota(all_indices.begin(), all_indices.end(), 0);
        std::shuffle(all_indices.begin(), all_indices.end(), m_rng);

        for (size_t i = 0; i < total_samples; ++i) {
            const auto& sample = m_strategy_memory[all_indices[i]];
            all_cards_cpu[i] = sample.infoset.getCardTensors();
            all_bets_cpu[i] = sample.infoset.getBetTensor();
            all_weights_cpu[i] = sample.weight;

            // Prepare targets and masks
            std::vector<float> targets(GameType::MAX_ACTIONS, 0.0f);
            std::vector<float> masks(GameType::MAX_ACTIONS, 0.0f);

            for (size_t j = 0; j < sample.legal_action_indices.size(); ++j) {
                int action_idx = sample.legal_action_indices[j];
                targets[action_idx] = sample.strategy[j];
                masks[action_idx] = 1.0f;
            }

            all_targets_cpu[i] = targets;
            all_masks_cpu[i] = masks;
        }

        // Bulk GPU transfer using torch::stack
        std::vector<torch::Tensor> all_cards_batched(GameType::NUM_CARD_TYPES);
        for (int card_type = 0; card_type < GameType::NUM_CARD_TYPES; ++card_type) {
            std::vector<torch::Tensor> cards_for_type;
            cards_for_type.reserve(total_samples);
            for (size_t i = 0; i < total_samples; ++i) {
                cards_for_type.push_back(all_cards_cpu[i][card_type]);
            }
            all_cards_batched[card_type] = torch::stack(cards_for_type).squeeze(1).to(m_device);
        }

        auto all_bets_batched = torch::stack(all_bets_cpu).squeeze(1).to(m_device);
        auto all_weights_batched = torch::from_blob(all_weights_cpu.data(), {static_cast<long>(total_samples), 1}).clone().to(m_device);

        // Convert targets and masks to tensors
        torch::Tensor all_targets_batched = torch::zeros({static_cast<long>(total_samples), GameType::MAX_ACTIONS}, m_device);
        torch::Tensor all_masks_batched = torch::zeros({static_cast<long>(total_samples), GameType::MAX_ACTIONS}, m_device);

        for (size_t i = 0; i < total_samples; ++i) {
            for (int j = 0; j < GameType::MAX_ACTIONS; ++j) {
                all_targets_batched[i][j] = all_targets_cpu[i][j];
                all_masks_batched[i][j] = all_masks_cpu[i][j];
            }
        }

        // Training loop
        for (int iter = 0; iter < SGD_ITERATIONS; ++iter) {
            auto t1 = std::chrono::high_resolution_clock::now();
            size_t batch_start = iter * BATCH_SIZE;
            size_t batch_end = std::min(batch_start + BATCH_SIZE, total_samples);

            // Skip empty batches
            if (batch_start >= total_samples || batch_end <= batch_start) {
                continue;
            }

            // Create batch tensors
            std::vector<torch::Tensor> batch_cards;
            for (int i = 0; i < GameType::NUM_CARD_TYPES; ++i) {
                batch_cards.push_back(all_cards_batched[i].slice(0, batch_start, batch_end));
            }
            auto batch_bets = all_bets_batched.slice(0, batch_start, batch_end);
            auto batch_targets = all_targets_batched.slice(0, batch_start, batch_end);
            auto batch_masks = all_masks_batched.slice(0, batch_start, batch_end);
            auto batch_weights = all_weights_batched.slice(0, batch_start, batch_end);

            // Get raw logits from the network
            auto logits = m_strategy_network->forward(batch_cards, batch_bets);

            // Mask illegal actions before calculating loss
            auto illegal_action_mask = (batch_masks == 0);
            logits = logits.masked_fill_(illegal_action_mask, -1e9);

            // Calculate cross-entropy loss for soft targets
            auto log_probabilities = torch::log_softmax(logits, -1);
            auto loss_per_element = -(batch_targets * log_probabilities);
            auto weighted_loss_per_element = loss_per_element * batch_masks * batch_weights;
            auto masked_loss = weighted_loss_per_element.sum() / ((batch_masks * batch_weights).sum() + 1e-9);

            // Backprop
            m_strategy_optimizer.zero_grad();
            masked_loss.backward();
            m_strategy_network->clip_gradients(GRADIENT_CLIP_NORM);
            m_strategy_optimizer.step();

            auto t2 = std::chrono::high_resolution_clock::now();
            auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

            std::cout << "Strategy network training iter " << iter
                      << ", loss: " << masked_loss.template item<float>()
                      << " time: " << ms_int.count() << "ms" << std::endl;
        }
    }

    // Member variables
    std::mt19937 m_rng;
    torch::Device m_device;
    std::array<DeepCFRModel, 2> m_advantage_networks{nullptr, nullptr};
    DeepCFRModel m_strategy_network{nullptr};
    std::vector<torch::optim::Adam> m_advantage_optimizers;
    torch::optim::Adam m_strategy_optimizer;
    std::array<AdvantageMemoryBuffer<GameType>, 2> m_adv_memories;
    std::vector<TrainingSampleStrategy> m_strategy_memory;

    // Constants
    static constexpr size_t BATCH_SIZE = 10000;
    static constexpr size_t MEMORY_SIZE = 40000000;
    static constexpr float LEARNING_RATE = 0.001f;
    static constexpr int SGD_ITERATIONS = 4000;
    static constexpr double GRADIENT_CLIP_NORM = 1.0;
    static constexpr int K_TRAVERSALS = 10000;
};

#endif // COROUTINE_DEEPCFR_HPP