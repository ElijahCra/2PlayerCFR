//
// Created by Elijah Crain on 8/10/25.
//

#ifndef COROUTINEAWAITABLES_HPP
#define COROUTINEAWAITABLES_HPP

#include <coroutine>
#include <future>
#include <vector>
#include <atomic>
#include <memory>
#include "GPUDispatcher.hpp"
#include "ThreadPool.hpp"

// Awaitable for a single GPU inference result
auto await_gpu_inference(GPUDispatcher& dispatcher, int player_idx, std::vector<torch::Tensor>&& cards, torch::Tensor&& bets) {
    struct GpuAwaiter {
        std::future<torch::Tensor> future;

        bool await_ready() const noexcept { return false; } // Always suspend

        void await_suspend(std::coroutine_handle<>) noexcept {
            // The future is already running, so we just wait for it.
            // Suspension is handled by the future itself.
        }

        torch::Tensor await_resume() { return future.get(); }
    };

    auto request = std::make_unique<ForwardRequest>();
    request->player_index = player_idx;
    request->cards = std::move(cards);
    request->bets = std::move(bets);

    return GpuAwaiter{ dispatcher.submit(std::move(request)) };
}

// Awaitable for waiting on multiple futures (e.g., from child traversals)
template<typename T>
auto await_all(std::vector<std::future<T>>&& futures) {
    struct WhenAllAwaiter {
        std::vector<std::future<T>> futures;

        bool await_ready() const noexcept { return futures.empty(); }

        void await_suspend(std::coroutine_handle<> h) {
            // A simple approach: spawn a new thread to wait for all futures
            // and then resume the coroutine. A more advanced implementation
            // could use a thread pool.
            std::thread([this, h]() {
                for (auto& f : futures) {
                    f.wait();
                }
                h.resume();
            }).detach();
        }

        std::vector<T> await_resume() {
            std::vector<T> results;
            results.reserve(futures.size());
            for (auto& f : futures) {
                results.push_back(f.get());
            }
            return results;
        }
    };
    return WhenAllAwaiter{ std::move(futures) };
}

#endif //COROUTINEAWAITABLES_HPP
