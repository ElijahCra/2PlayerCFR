//
// Created by elijah on 8/4/25.
//

#ifndef TYPES_HPP
#define TYPES_HPP

#include <future>
#include <vector>
#include "torch/torch.h"

// A struct to hold the components of an information set.
// Tensors are kept on the CPU until batched for training.
struct InfoSet
{
    std::vector<torch::Tensor> cardTensors;
    torch::Tensor betTensor;

    [[nodiscard]] std::vector<torch::Tensor> getCardTensors() const { return cardTensors; }
    [[nodiscard]] torch::Tensor getBetTensor() const { return betTensor; }
};

// A struct for storing samples in the advantage replay buffer.
struct TrainingSampleAdvantage {
    InfoSet infoset;
    int iteration;
    std::vector<float> advantages;  // r_tilde(I, a) for each legal action
    std::vector<int> legal_action_indices;
    float weight;                   // Weight for Linear CFR (typically the iteration number)
};

// A struct for storing samples in the strategy replay buffer.
struct TrainingSampleStrategy
{
    InfoSet infoset;
    int iteration;
    std::vector<float> strategy;
    std::vector<int> legal_action_indices;
    float weight;
};
struct ForwardRequest {
    int player_index;                   // Which advantage network to use (0 or 1)
    std::vector<torch::Tensor> cards;   // Card tensors (on CPU)
    torch::Tensor bets;                 // Bet tensor (on CPU)
    std::promise<torch::Tensor> promise;// Promise to fulfill with the GPU result
};


template <typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }

    // Waits until an item is available and returns it
    T pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] { return !m_queue.empty(); });
        T value = std::move(m_queue.front());
        m_queue.pop();
        return value;
    }

    // Tries to pop an item, but returns std::nullopt if the queue is empty
    // after the specified timeout.
    std::optional<T> pop_with_timeout(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_cond.wait_for(lock, timeout, [this] { return !m_queue.empty(); })) {
            T value = std::move(m_queue.front());
            m_queue.pop();
            return value;
        }
        return std::nullopt;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};

// Training constants from the paper
static constexpr size_t BATCH_SIZE = 10000;
static constexpr size_t MEMORY_SIZE = 40000000; // 40 million
static constexpr float LEARNING_RATE = 0.001f;
static constexpr int SGD_ITERATIONS = 4000; // SGD iterations per training step
static constexpr double GRADIENT_CLIP_NORM = 1.0;
static constexpr int K_TRAVERSALS = 10000; // Number of traversals per iteration

#endif //TYPES_HPP
