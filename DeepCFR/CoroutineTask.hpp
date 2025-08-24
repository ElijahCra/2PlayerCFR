#pragma once
#include <coroutine>
#include <exception>
#include <optional>
#include "torch/torch.h"

template<typename T>
struct CoroutineTask {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    handle_type m_handle;

    explicit CoroutineTask(handle_type handle) : m_handle(handle) {}
    ~CoroutineTask() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    // A task is a move-only type
    CoroutineTask(const CoroutineTask&) = delete;
    CoroutineTask& operator=(const CoroutineTask&) = delete;
    CoroutineTask(CoroutineTask&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    CoroutineTask& operator=(CoroutineTask&& other) noexcept {
        if (this != &other) {
            if (m_handle) m_handle.destroy();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    struct promise_type {
        std::optional<T> m_value;
        std::exception_ptr m_exception;
        // This will hold the result from the GPU, placed here by the GPUDispatcher
        torch::Tensor m_gpu_result; 

        CoroutineTask<T> get_return_object() {
            return CoroutineTask<T>{handle_type::from_promise(*this)};
        }

        // Start tasks lazily. The scheduler will explicitly resume them. [cite: 26]
        std::suspend_always initial_suspend() noexcept { return {}; }

        // Keep the frame alive after completion until the scheduler cleans it up. [cite: 29]
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { m_exception = std::current_exception(); }

        template<std::convertible_to<T> From>
        void return_value(From&& from) {
            m_value = std::forward<From>(from);
        }
    };
};