//
// Created by elijah on 8/4/25.
//

#ifndef TYPES_HPP
#define TYPES_HPP

#include <future>
#include <vector>
#include "torch/torch.h"
#include <random>

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
    std::vector<float> advantages;  // r_tilde(I, a) for each legal action
    std::vector<int> legal_action_indices;
    float weight;                   // Weight for Linear CFR (typically the iteration number)
};

// A struct for storing samples in the strategy replay buffer.
struct TrainingSampleStrategy
{
    InfoSet infoset;
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


inline std::vector<float> compute_strategy_from_advantages(const std::vector<float>& advantages) {
    std::vector<float> strategy(advantages.size());

    // Compute positive regrets
    std::vector<float> positive_regrets(advantages.size());
    float sum_positive = 0.0f;

    for (size_t i = 0; i < advantages.size(); ++i) {
        positive_regrets[i] = std::max(0.0f, advantages[i]);
        sum_positive += positive_regrets[i];
    }

    if (sum_positive > 0) {
        // Regret matching
        for (size_t i = 0; i < advantages.size(); ++i) {
            strategy[i] = positive_regrets[i] / sum_positive;
        }
    } else {
        // Uniform strategy when all regrets are negative
        float uniform_prob = 1.0f / advantages.size();
        std::fill(strategy.begin(), strategy.end(), uniform_prob);
    }

    return strategy;
}


template<typename T>
void add_to_strategy_memory(std::vector<T>& memory, const T& sample, size_t max_size, std::mt19937& rng) {
    if (memory.size() < max_size) {
        memory.push_back(sample);
    } else {
        // Reservoir sampling
        std::cout << "reservoir" << std::endl;
        std::uniform_int_distribution<size_t> dist(0, memory.size());
        size_t idx = dist(rng);
        if (idx < max_size) {
            memory[idx] = sample;
        }
    }
}
#endif //TYPES_HPP
