#ifndef COROUTINE_TRAVERSAL_HPP
#define COROUTINE_TRAVERSAL_HPP

#include <coroutine>
#include <memory>
#include <optional>
#include <vector>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <torch/torch.h>

#include "HybridAdvantageStorage.hpp"
#include "Net.hpp"

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

    GPUBatchProcessor(torch::Device device) : m_networks(nullptr), m_device(device)
    {
    }

    void init(std::array<DeepCFRModel, 2>* networks)
    {
        m_networks = networks;
        m_stop = false;
        for (auto& network : *m_networks){
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
            results = m_networks->data()[player]->forward(batched_cards, batched_bets);
        }

        // --- 5. Move results back to CPU at once ---
        auto results_cpu = results.to(torch::kCPU);

        // --- 6. Distribute results ---
        for (size_t i = 0; i < batch_size; ++i) {
            batch[i]->result = results_cpu.slice(0, i, i + 1).clone();
            batch[i]->ready.store(true);
        }
    }



    std::array<DeepCFRModel, 2>* m_networks;
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




#endif // COROUTINE_TRAVERSAL_HPP