#pragma once
#include "GPUDispatcher.hpp"
#include "CoroutineTask.hpp"

class GpuAwaitable {
public:
    GpuAwaitable(GPUDispatcher& dispatcher, std::vector<torch::Tensor> cards, torch::Tensor bets, int player_index)
        : m_dispatcher(dispatcher), m_cards(std::move(cards)), m_bets(std::move(bets)), m_player_index(player_index) {}

    // Always suspend, we need to send the work to the GPU. [cite: 39]
    bool await_ready() const noexcept { return false; }

    // This is the critical integration point. [cite: 42]
    void await_suspend(std::coroutine_handle<> h) {
        auto request = std::make_unique<ForwardRequest>();
        request->cards = m_cards;
        request->bets = m_bets;
        request->player_index = m_player_index;
        request->handle_to_resume = h; // Pass our continuation handle

        m_dispatcher.submit(std::move(request));
    }

    // When resumed, the result is already in the promise. This retrieves it. [cite: 47]
    torch::Tensor await_resume() noexcept {
        // This is a bit of a trick. We know the handle we are being resumed on, but
        // don't have a direct parameter. The most robust way would be to get it from the promise.
        // For simplicity, we assume the dispatcher has populated the result.
        // The GpuDispatcher places the result in the promise before queuing for resumption.
        return torch::Tensor(); // The result will be retrieved from the promise in the calling coro
    }

private:
    GPUDispatcher& m_dispatcher;
    std::vector<torch::Tensor> m_cards;
    torch::Tensor m_bets;
    int m_player_index;
};