//
// Created by elijah on 8/11/25.
//

#ifndef COROUTINE_HPP
#define COROUTINE_HPP
#include <coroutine>
#include <iostream>
#include <vector>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <variant>

// Forward declarations
class Task;
class ThreadPool;

// Global thread pool (in production, inject this dependency)
inline ThreadPool* g_thread_pool = nullptr;

// Node types
enum class NodeType {
    Terminal,
    Chance,
    Player
};

struct Node {
    NodeType type;
    double utility_value;  // For terminal nodes
    std::vector<std::unique_ptr<Node>> children;

    Node(NodeType t) : type(t), utility_value(0.0) {}
};

// Result type for tree traversal
struct TraversalResult {
    double value;
    // Additional game state info could go here
};

// GPU Request placeholder
class GPURequest {
public:
    // Placeholder for GPU request implementation
    void submit() {
        // GPU submission logic here
    }

    bool is_ready() const {
        // Check if GPU computation is complete
        return true; // Simplified
    }

    std::vector<int> get_available_decisions() {
        // Return available player decisions after GPU computation
        return {0, 1, 2}; // Example decisions
    }
};

// Thread pool for work stealing
class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                work_loop();
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto& worker : workers) {
            worker.join();
        }
    }

    void enqueue_coroutine(std::coroutine_handle<> handle) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            ready_coroutines.push(handle);
        }
        cv.notify_one();
    }

    bool try_steal_work(std::coroutine_handle<>& handle) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (!ready_coroutines.empty()) {
            handle = ready_coroutines.front();
            ready_coroutines.pop();
            return true;
        }
        return false;
    }

private:
    void work_loop() {
        while (true) {
            std::coroutine_handle<> handle;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this] { return stop || !ready_coroutines.empty(); });

                if (stop && ready_coroutines.empty()) {
                    return;
                }

                if (!ready_coroutines.empty()) {
                    handle = ready_coroutines.front();
                    ready_coroutines.pop();
                }
            }

            if (handle) {
                handle.resume();
            }
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::coroutine_handle<>> ready_coroutines;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop;
};

// Awaitable for GPU requests with work stealing
class GPUAwaitable {
public:
    GPUAwaitable(GPURequest& req) : request(req), ready(false) {
        request.submit();
    }

    bool await_ready() {
        // Check if we can proceed without suspending
        ready = request.is_ready();
        return ready;
    }

    void await_suspend(std::coroutine_handle<> handle) {
        // Store handle for later resumption by any thread
        suspended_handle = handle;

        // Add to thread pool for work stealing
        // Any available thread can pick this up when GPU is ready
        g_thread_pool->enqueue_coroutine(handle);
    }

    std::vector<int> await_resume() {
        // Return the GPU computation result
        return request.get_available_decisions();
    }

private:
    GPURequest& request;
    std::atomic<bool> ready;
    std::coroutine_handle<> suspended_handle;
};

// Task type for coroutines
class Task {
public:
    struct promise_type {
        TraversalResult result;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        struct final_awaiter {
            bool await_ready() noexcept { return false; }

            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().continuation) {
                    // Resume parent coroutine
                    h.promise().continuation.resume();
                }
            }

            void await_resume() noexcept {}
        };

        final_awaiter final_suspend() noexcept { return {}; }

        void unhandled_exception() {
            exception = std::current_exception();
        }

        void return_value(TraversalResult val) {
            result = val;
        }
    };

    // Awaiter for co_await on Task
    struct awaiter {
        std::coroutine_handle<promise_type> coro;

        bool await_ready() {
            return coro.done();
        }

        void await_suspend(std::coroutine_handle<> handle) {
            coro.promise().continuation = handle;
        }

        TraversalResult await_resume() {
            if (coro.promise().exception) {
                std::rethrow_exception(coro.promise().exception);
            }
            return coro.promise().result;
        }
    };

    awaiter operator co_await() {
        return awaiter{coro};
    }

    Task(std::coroutine_handle<promise_type> h) : coro(h) {}

    ~Task() {
        if (coro) {
            coro.destroy();
        }
    }

    // Move constructor/assignment
    Task(Task&& other) noexcept : coro(std::exchange(other.coro, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (coro) coro.destroy();
            coro = std::exchange(other.coro, {});
        }
        return *this;
    }

    // No copy
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    TraversalResult get() {
        if (!coro.done()) {
            // Block until complete (for root call)
            while (!coro.done()) {
                std::this_thread::yield();
            }
        }
        if (coro.promise().exception) {
            std::rethrow_exception(coro.promise().exception);
        }
        return coro.promise().result;
    }

private:
    std::coroutine_handle<promise_type> coro;
};

// Game state transition (placeholder)
void transition_game_state(Node* node) {
    // Implement game state transition logic
    // This modifies game state based on chance node
}

// Main tree traversal coroutine
Task traverse_tree(Node* node) {
    if (!node) {
        co_return TraversalResult{0.0};
    }

    switch (node->type) {
        case NodeType::Terminal: {
            // Terminal node: return utility value
            co_return TraversalResult{node->utility_value};
        }

        case NodeType::Chance: {
            // Chance node: transition game and continue descending
            transition_game_state(node);

            // Aggregate results from all children
            double total_value = 0.0;
            std::vector<Task> child_tasks;

            for (auto& child : node->children) {
                child_tasks.push_back(traverse_tree(child.get()));
            }

            // Wait for all children
            for (auto& task : child_tasks) {
                auto result = co_await task;
                total_value += result.value;
            }

            // Could apply probability weights here
            co_return TraversalResult{total_value / node->children.size()};
        }

        case NodeType::Player: {
            // Player node: submit GPU request and suspend
            GPURequest gpu_request;

            // This will suspend if GPU not ready, allowing work stealing
            auto decisions = co_await GPUAwaitable(gpu_request);

            // Now we have GPU results, traverse selected children
            double best_value = -std::numeric_limits<double>::infinity();
            std::vector<Task> decision_tasks;

            for (int decision : decisions) {
                if (decision < node->children.size()) {
                    decision_tasks.push_back(traverse_tree(node->children[decision].get()));
                }
            }

            // Evaluate all decisions (could be parallel)
            for (auto& task : decision_tasks) {
                auto result = co_await task;
                best_value = std::max(best_value, result.value);
            }

            co_return TraversalResult{best_value};
        }
    }

    co_return TraversalResult{0.0};
}


// Main function demonstrating usage
int main() {
    // Initialize thread pool
    ThreadPool pool(std::thread::hardware_concurrency());
    g_thread_pool = &pool;

    // Start traversal
    auto task = traverse_tree(tree.get());

    // Get result (blocks until complete)
    auto result = task.get();

    std::cout << "Tree traversal result: " << result.value << std::endl;

    return 0;
}
#endif //COROUTINE_HPP
