//
// Created by Elijah Crain on 8/17/25.
//

// CoroutineTraversal.hpp
#ifndef COROUTINETRAVERSAL_HPP
#define COROUTINETRAVERSAL_HPP

#include <coroutine>
#include <memory>
#include <queue>
#include <optional>
#include <chrono>
#include "types.hpp"
#include "GPUDispatcher.hpp"

template<typename GameType>
class TraversalCoroutine {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        float result_value = 0.0f;
        std::exception_ptr exception;

        TraversalCoroutine get_return_object() {
            return TraversalCoroutine{handle_type::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(float value) { result_value = value; }

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    explicit TraversalCoroutine(handle_type h) : coro(h) {}

    ~TraversalCoroutine() {
        if (coro) coro.destroy();
    }

    // Move only
    TraversalCoroutine(TraversalCoroutine&& other) noexcept : coro(std::exchange(other.coro, {})) {}
    TraversalCoroutine& operator=(TraversalCoroutine&& other) noexcept {
        if (this != &other) {
            if (coro) coro.destroy();
            coro = std::exchange(other.coro, {});
        }
        return *this;
    }

    bool done() const { return coro.done(); }
    void resume() { coro.resume(); }
    float get_result() const { return coro.promise().result_value; }

private:
    handle_type coro;
};

// Awaitable for GPU forward pass
template<typename GameType>
struct GPUForwardAwaitable {
    GPUDispatcher& dispatcher;
    int player_index;
    std::vector<torch::Tensor> cards;
    torch::Tensor bets;
    std::future<torch::Tensor> future;

    GPUForwardAwaitable(GPUDispatcher& disp, int player,
                        std::vector<torch::Tensor> c, torch::Tensor b)
        : dispatcher(disp), player_index(player), cards(std::move(c)), bets(std::move(b)) {}

    bool await_ready() { return false; } // Always suspend

    void await_suspend(std::coroutine_handle<> h) {
        auto request = std::make_unique<ForwardRequest>();
        request->player_index = player_index;
        request->cards = cards;
        request->bets = bets;
        future = dispatcher.submit(std::move(request));
    }

    torch::Tensor await_resume() {
        return future.get();
    }
};

// Work item representing a suspended traversal
template<typename GameType>
struct WorkItem {
    std::coroutine_handle<> handle;
    std::future<torch::Tensor> gpu_future;
    std::chrono::steady_clock::time_point submit_time;

    bool is_ready() const {
        return gpu_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
};
#endif
