//
// Created by elijah on 8/4/25.
//

#ifndef TYPES_HPP
#define TYPES_HPP

#include <future>
#include <vector>
#include "torch/torch.h"

// Forward request for GPU processing
struct ForwardRequest {
    int player_index;
    std::vector<torch::Tensor> cards;
    torch::Tensor bets;
    std::promise<torch::Tensor> promise;
};

// Thread-safe queue for GPU requests
template<typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(item));
        }
        m_cv.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || m_stop; });

        if (m_stop && m_queue.empty()) {
            return std::nullopt;
        }

        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    std::optional<T> pop_with_timeout(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        bool result = m_cv.wait_for(lock, timeout, [this] { return !m_queue.empty() || m_stop; });

        if (!result || (m_stop && m_queue.empty())) {
            return std::nullopt;
        }

        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<T> m_queue;
    bool m_stop = false;
};

// Info set representation
struct InfoSet {
    std::vector<torch::Tensor> card_tensors;
    torch::Tensor bet_tensor;

    std::vector<torch::Tensor> getCardTensors() const { return card_tensors; }
    torch::Tensor getBetTensor() const { return bet_tensor; }
};

// Training sample for advantage network
struct TrainingSampleAdvantage {
    InfoSet infoset;
    int iteration;
    std::vector<int> legal_action_indices;
    std::vector<float> advantages;
    float weight;
};

// Training sample for strategy network
struct TrainingSampleStrategy {
    InfoSet infoset;
    int iteration;
    std::vector<int> legal_action_indices;
    std::vector<float> strategy;
    float weight;
};

// Batch data for training
struct BatchData {
    std::vector<torch::Tensor> cards;
    torch::Tensor bets;
    torch::Tensor targets;
    torch::Tensor masks;
    torch::Tensor weights;
    bool is_valid = true;
};

// Statistics for tracking performance
struct TraversalStats {
    std::atomic<size_t> total_traversals{0};
    std::atomic<size_t> gpu_requests{0};
    std::atomic<size_t> gpu_batch_count{0};
    std::atomic<double> total_gpu_wait_time{0.0};
    std::atomic<double> total_traversal_time{0.0};

    void reset() {
        total_traversals = 0;
        gpu_requests = 0;
        gpu_batch_count = 0;
        total_gpu_wait_time = 0.0;
        total_traversal_time = 0.0;
    }

    void print() const {
        std::cout << "=== Traversal Statistics ===" << std::endl;
        std::cout << "Total traversals: " << total_traversals << std::endl;
        std::cout << "GPU requests: " << gpu_requests << std::endl;
        std::cout << "GPU batches processed: " << gpu_batch_count << std::endl;

        if (gpu_requests > 0) {
            std::cout << "Avg GPU wait time: "
                     << (total_gpu_wait_time / gpu_requests) << " ms" << std::endl;
            std::cout << "Avg batch size: "
                     << (static_cast<double>(gpu_requests) / gpu_batch_count) << std::endl;
        }

        if (total_traversals > 0) {
            std::cout << "Avg traversal time: "
                     << (total_traversal_time / total_traversals) << " ms" << std::endl;
        }
    }
};

#endif //TYPES_HPP
