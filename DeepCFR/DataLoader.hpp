//
// Created by elijah on 8/4/25.
//

#ifndef DATALOADER_HPP
#define DATALOADER_HPP

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>

#include "torch/torch.h"
#include "types.hpp"

// A generic, asynchronous data loader for our training samples.
// It prepares batches in a background thread to keep the GPU fed.
template<typename GameType>
class DataLoader {
public:
    // A struct to hold a single, fully-prepared batch of tensors on the target device.
    struct Batch {
        std::vector<torch::Tensor> cards;
        torch::Tensor bets;
        torch::Tensor targets;
        torch::Tensor masks;
        torch::Tensor weights;
        bool is_valid = true; // Flag to signal the end of the data stream
    };

    // Constructor
    DataLoader(const AdvantageMemoryBuffer<GameType>& memory_buffer,
               size_t batch_size,
               int total_batches,
               torch::Device device,
               std::mt19937& rng)
        : m_memory_buffer(memory_buffer),
          m_batch_size(batch_size),
          m_total_batches(total_batches),
          m_device(device),
          m_rng(rng) {}

    // Destructor ensures the worker thread is properly joined.
    ~DataLoader() {
        if (m_worker_thread.joinable()) {
            stop();
        }
    }

    // Starts the background worker thread.
    void start() {
        m_worker_thread = std::thread(&DataLoader<GameType>::worker_loop, this);
    }

    // Signals the worker thread to stop and waits for it to finish.
    void stop() {
        m_stop_flag = true;
        m_cond_producer.notify_one(); // Wake up producer if it's waiting
        m_cond_consumer.notify_one(); // Wake up consumer if it's waiting
        if (m_worker_thread.joinable()) {
            m_worker_thread.join();
        }
    }

    // Fetches a pre-cooked batch from the queue. Blocks if the queue is empty.
    Batch get_batch() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_consumer.wait(lock, [this] { return !m_queue.empty() || m_stop_flag; });

        if (m_stop_flag && m_queue.empty()) {
            return { .is_valid = false };
        }

        Batch batch = std::move(m_queue.front());
        m_queue.pop();
        lock.unlock();
        m_cond_producer.notify_one(); // Signal producer that there's space in the queue
        return batch;
    }

private:
    // The main function for the background worker thread.
    void worker_loop();


    const AdvantageMemoryBuffer<GameType>& m_memory_buffer;

    size_t m_batch_size;
    int m_total_batches;
    torch::Device m_device;
    std::mt19937& m_rng;

    std::thread m_worker_thread;
    std::queue<Batch> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond_producer;
    std::condition_variable m_cond_consumer;
    std::atomic<bool> m_stop_flag{false};
    const size_t m_max_queue_size = 10; // Number of batches to pre-fetch
};

template<typename GameType>
void DataLoader<GameType>::worker_loop() {
    std::vector<int> shuffled_indices(m_memory_buffer.size());
    std::iota(shuffled_indices.begin(), shuffled_indices.end(), 0);
    std::shuffle(shuffled_indices.begin(), shuffled_indices.end(), m_rng);

    for (int i = 0; i < m_total_batches; ++i) {
        if (m_stop_flag) return;

        int batch_start_idx = i * m_batch_size;
        int current_batch_size = std::min(m_batch_size, m_memory_buffer.size() - batch_start_idx);
        if (current_batch_size <= 0) break;

        Batch batch;

        // --- The core optimization ---
        // Batch creation is now just a series of fast slice operations.
        // No more looping, no more stacking.
        const auto& card_tensors = m_memory_buffer.get_card_tensors();
        for(const auto& tensor : card_tensors) {
            batch.cards.push_back(tensor.slice(0, batch_start_idx, batch_start_idx + current_batch_size).to(m_device, true));
        }

        batch.bets = m_memory_buffer.get_bet_tensors().slice(0, batch_start_idx, batch_start_idx + current_batch_size).to(m_device, true);
        batch.targets = m_memory_buffer.get_targets().slice(0, batch_start_idx, batch_start_idx + current_batch_size).to(m_device, true);
        batch.masks = m_memory_buffer.get_masks().slice(0, batch_start_idx, batch_start_idx + current_batch_size).to(m_device, true);
        batch.weights = m_memory_buffer.get_weights().slice(0, batch_start_idx, batch_start_idx + current_batch_size).to(m_device, true);

        // --- Push the prepared batch to the queue ---
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_producer.wait(lock, [this] { return m_queue.size() < m_max_queue_size || m_stop_flag; });
        if (m_stop_flag) return;

        m_queue.push(std::move(batch));
        lock.unlock();
        m_cond_consumer.notify_one();
    }

    // Signal that production is finished.
    std::unique_lock<std::mutex> lock(m_mutex);
    m_stop_flag = true;
    lock.unlock();
    m_cond_consumer.notify_all();
}



#endif //DATALOADER_HPP
