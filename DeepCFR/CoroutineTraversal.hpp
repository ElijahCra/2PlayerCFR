// CoroutineTraversal.hpp
#ifndef COROUTINETRAVERSAL_HPP
#define COROUTINETRAVERSAL_HPP

#include <coroutine>
#include <memory>
#include <queue>
#include <optional>
#include <chrono>
#include <atomic>
#include "types.hpp"
#include "GPUDispatcher.hpp"

// Enhanced coroutine scheduler with work stealing
template<typename GameType>
class CoroutineScheduler {
public:
    struct GPURequest {
        std::coroutine_handle<> handle;
        std::future<torch::Tensor> future;
        std::chrono::steady_clock::time_point submit_time;
    };

    explicit CoroutineScheduler(GPUDispatcher& dispatcher, size_t max_concurrent = 128)
        : m_dispatcher(dispatcher), m_max_concurrent(max_concurrent) {}

    // Submit a GPU request and suspend the coroutine
    struct GPUAwaitable {
        CoroutineScheduler& scheduler;
        int player_index;
        std::vector<torch::Tensor> cards;
        torch::Tensor bets;
        std::future<torch::Tensor> future;

        bool await_ready() {
            // Never ready immediately - always suspend for batching
            return false;
        }

        void await_suspend(std::coroutine_handle<> h) {
            // Create and submit GPU request
            auto request = std::make_unique<ForwardRequest>();
            request->player_index = player_index;
            request->cards = std::move(cards);
            request->bets = std::move(bets);

            future = scheduler.m_dispatcher.submit(std::move(request));

            // Register with scheduler
            scheduler.register_gpu_request(h, std::move(future));
        }

        torch::Tensor await_resume() {
            // Future should already be ready when we resume
            return future.get();
        }
    };

    // Create an awaitable for GPU operations
    GPUAwaitable submit_gpu_request(int player, std::vector<torch::Tensor> cards, torch::Tensor bets) {
        return GPUAwaitable{*this, player, std::move(cards), std::move(bets), {}};
    }

    // Register a suspended coroutine waiting for GPU
    void register_gpu_request(std::coroutine_handle<> handle, std::future<torch::Tensor> future) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gpu_waiting.push_back({handle, std::move(future), std::chrono::steady_clock::now()});
    }

    // Check for completed GPU requests and resume coroutines
    size_t process_ready_gpu_requests() {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t resumed_count = 0;

        auto it = m_gpu_waiting.begin();
        while (it != m_gpu_waiting.end()) {
            if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                // GPU result ready - resume coroutine
                m_ready_queue.push(it->handle);
                it = m_gpu_waiting.erase(it);
                resumed_count++;
            } else {
                ++it;
            }
        }

        return resumed_count;
    }

    // Get next ready coroutine to resume
    std::optional<std::coroutine_handle<>> get_ready_coroutine() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_ready_queue.empty()) {
            return std::nullopt;
        }

        auto handle = m_ready_queue.front();
        m_ready_queue.pop();
        return handle;
    }

    // Add a new coroutine to the ready queue
    void enqueue_ready(std::coroutine_handle<> handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ready_queue.push(handle);
    }

    size_t waiting_count() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_gpu_waiting.size();
    }

    size_t ready_count() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_ready_queue.size();
    }

    // Statistics
    struct Stats {
        std::atomic<size_t> total_gpu_requests{0};
        std::atomic<size_t> total_gpu_completions{0};
        std::atomic<double> total_gpu_wait_ms{0.0};
        std::atomic<size_t> max_concurrent_gpu{0};

        void print() const {
            std::cout << "=== Scheduler Statistics ===" << std::endl;
            std::cout << "GPU requests: " << total_gpu_requests << std::endl;
            std::cout << "GPU completions: " << total_gpu_completions << std::endl;
            if (total_gpu_completions > 0) {
                std::cout << "Avg GPU wait: "
                         << (total_gpu_wait_ms / total_gpu_completions) << " ms" << std::endl;
            }
            std::cout << "Max concurrent GPU: " << max_concurrent_gpu << std::endl;
        }
    } stats;

private:
    GPUDispatcher& m_dispatcher;
    size_t m_max_concurrent;

    mutable std::mutex m_mutex;
    std::vector<GPURequest> m_gpu_waiting;
    std::queue<std::coroutine_handle<>> m_ready_queue;
};

// Optimized work-stealing deque for coroutine scheduling
template<typename T>
class WorkStealingDeque {
public:
    void push_bottom(T item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_deque.push_back(std::move(item));
    }

    std::optional<T> pop_bottom() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_deque.empty()) return std::nullopt;

        T item = std::move(m_deque.back());
        m_deque.pop_back();
        return item;
    }

    std::optional<T> steal_top() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_deque.empty()) return std::nullopt;

        T item = std::move(m_deque.front());
        m_deque.pop_front();
        return item;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_deque.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_deque.size();
    }

private:
    mutable std::mutex m_mutex;
    std::deque<T> m_deque;
};

// Multi-threaded coroutine executor with work stealing
template<typename GameType>
class CoroutineExecutor {
public:
    using Task = std::coroutine_handle<>;

    explicit CoroutineExecutor(size_t num_threads = std::thread::hardware_concurrency())
        : m_num_threads(num_threads), m_stop(false) {
        m_work_queues.resize(num_threads);
    }

    ~CoroutineExecutor() {
        stop();
    }

    void start() {
        for (size_t i = 0; i < m_num_threads; ++i) {
            m_threads.emplace_back(&CoroutineExecutor::worker_loop, this, i);
        }
    }

    void stop() {
        m_stop = true;
        m_cv.notify_all();

        for (auto& thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void submit(Task task, size_t preferred_thread = 0) {
        preferred_thread = preferred_thread % m_num_threads;
        m_work_queues[preferred_thread].push_bottom(task);
        m_cv.notify_one();
    }

private:
    void worker_loop(size_t thread_id) {
        while (!m_stop) {
            // Try to get work from own queue
            auto task_opt = m_work_queues[thread_id].pop_bottom();

            // If no work, try stealing from others
            if (!task_opt.has_value()) {
                for (size_t i = 1; i < m_num_threads; ++i) {
                    size_t victim = (thread_id + i) % m_num_threads;
                    task_opt = m_work_queues[victim].steal_top();
                    if (task_opt.has_value()) break;
                }
            }

            if (task_opt.has_value()) {
                // Execute the coroutine
                auto task = task_opt.value();
                if (task && !task.done()) {
                    task.resume();
                }
            } else {
                // No work available, wait
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait_for(lock, std::chrono::milliseconds(1), [this, thread_id] {
                    return m_stop || !m_work_queues[thread_id].empty();
                });
            }
        }
    }

    size_t m_num_threads;
    std::vector<WorkStealingDeque<Task>> m_work_queues;
    std::vector<std::thread> m_threads;
    std::atomic<bool> m_stop;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

#endif // COROUTINETRAVERSAL_HPP