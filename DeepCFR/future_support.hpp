//
// Created by Elijah Crain on 8/10/25.
//

#ifndef FUTURE_SUPPORT_HPP
#define FUTURE_SUPPORT_HPP

#include <coroutine>
#include <future>
#include <exception>
#include <thread>

// Primary template for specializing coroutine_traits for std::future
template<typename T, typename... Args>
struct std::coroutine_traits<std::future<T>, Args...> {
    struct promise_type {
        std::promise<T> m_promise;

        // This is called by the compiler to get the return object for the caller.
        auto get_return_object() -> std::future<T> {
            return m_promise.get_future();
        }

        // The coroutine will start executing immediately.
        auto initial_suspend() const noexcept -> std::suspend_never {
            return {};
        }

        // The coroutine will clean up and destroy itself upon completion.
        auto final_suspend() const noexcept -> std::suspend_never {
            return {};
        }

        // This is called for 'co_return value;'
        void return_value(T value) {
            m_promise.set_value(std::move(value));
        }

        // This is called when an exception propagates out of the coroutine.
        void unhandled_exception() {
            m_promise.set_exception(std::current_exception());
        }
    };
};

// Specialization for std::future<void>
template<typename... Args>
struct std::coroutine_traits<std::future<void>, Args...> {
    struct promise_type {
        std::promise<void> m_promise;

        auto get_return_object() -> std::future<void> {
            return m_promise.get_future();
        }

        auto initial_suspend() const noexcept -> std::suspend_never {
            return {};
        }

        auto final_suspend() const noexcept -> std::suspend_never {
            return {};
        }

        // This is called for 'co_return;'
        void return_void() {
            m_promise.set_value();
        }

        void unhandled_exception() {
            m_promise.set_exception(std::current_exception());
        }
    };
};

namespace std {
// Define an awaiter for std::future
template <typename T>
struct future_awaiter {
    std::future<T> future;

    // await_ready: Check if we even need to suspend.
    // We don't suspend if the future is already finished.
    bool await_ready() const noexcept {
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    // await_suspend: What to do when we suspend.
    // We launch a thread to wait for the future to be ready, then resume the coroutine.
    void await_suspend(std::coroutine_handle<> h) const {
        std::thread([this, h] {
            future.wait();
            h.resume();
        }).detach();
    }

    // await_resume: What to return after we resume.
    // We get the value from the future.
    T await_resume() {
        return future.get();
    }
};

// Overload the co_await operator for std::future
template <typename T>
auto operator co_await(std::future<T>&& f) noexcept {
    return future_awaiter<T>{std::move(f)};
}
}

#endif // FUTURE_SUPPORT_HPP
