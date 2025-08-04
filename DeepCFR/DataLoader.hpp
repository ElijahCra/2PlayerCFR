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
    DataLoader(const std::vector<TrainingSampleAdvantage>& memory,
               size_t batch_size,
               int total_batches,
               torch::Device device,
               std::mt19937& rng)
        : m_memory(memory),
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

    const std::vector<TrainingSampleAdvantage>& m_memory;
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

// Implementation of the worker thread's loop
template<typename GameType>
void DataLoader<GameType>::worker_loop() {
    // Create a shuffled list of indices to draw samples from memory.
    std::vector<int> shuffled_indices(m_memory.size());
    std::iota(shuffled_indices.begin(), shuffled_indices.end(), 0);
    std::shuffle(shuffled_indices.begin(), shuffled_indices.end(), m_rng);

    for (int i = 0; i < m_total_batches; ++i) {
        if (m_stop_flag) return;

        int batch_start_idx = i * m_batch_size;
        int current_batch_size = std::min(m_batch_size, m_memory.size() - batch_start_idx);
        if (current_batch_size <= 0) break;

        // --- 1. Prepare batch data on the CPU ---
        std::vector<std::vector<torch::Tensor>> batch_cards_cpu(current_batch_size);
        std::vector<torch::Tensor> batch_bets_cpu(current_batch_size);
        std::vector<torch::Tensor> batch_targets_cpu(current_batch_size);
        std::vector<torch::Tensor> batch_masks_cpu(current_batch_size);
        std::vector<float> batch_weights_cpu(current_batch_size);

        for (int j = 0; j < current_batch_size; ++j) {
            const auto& sample = m_memory[shuffled_indices[batch_start_idx + j]];
            batch_cards_cpu[j] = sample.infoset.getCardTensors();
            batch_bets_cpu[j] = sample.infoset.getBetTensor();
            batch_weights_cpu[j] = sample.weight;

            torch::Tensor targets = torch::zeros({GameType::MAX_ACTIONS});
            torch::Tensor masks = torch::zeros({GameType::MAX_ACTIONS});

            for (size_t k = 0; k < sample.legal_action_indices.size(); ++k) {
                int action_idx = sample.legal_action_indices[k];
                targets[action_idx] = sample.advantages[k];
                masks[action_idx] = 1.0f;
            }
            batch_targets_cpu[j] = targets;
            batch_masks_cpu[j] = masks;
        }

        // --- 2. Stack, transfer to GPU, and create the final Batch object ---
        Batch batch;
        for (int card_type = 0; card_type < GameType::NUM_CARD_TYPES; ++card_type) {
            std::vector<torch::Tensor> cards_for_type;
            cards_for_type.reserve(current_batch_size);
            for(int k=0; k < current_batch_size; ++k) {
                cards_for_type.push_back(batch_cards_cpu[k][card_type]);
            }
            // Use non_blocking=true for asynchronous transfer
            batch.cards.push_back(torch::stack(cards_for_type).squeeze(1).to(m_device, true));
        }

        batch.bets = torch::stack(batch_bets_cpu).squeeze(1).to(m_device, true);
        batch.targets = torch::stack(batch_targets_cpu).to(m_device, true);
        batch.masks = torch::stack(batch_masks_cpu).to(m_device, true);
        batch.weights = torch::from_blob(batch_weights_cpu.data(), {(long)current_batch_size, 1}).clone().to(m_device, true);

        // --- 3. Push the prepared batch to the thread-safe queue ---
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_producer.wait(lock, [this] { return m_queue.size() < m_max_queue_size || m_stop_flag; });
        if (m_stop_flag) return;

        m_queue.push(std::move(batch));
        lock.unlock();
        m_cond_consumer.notify_one(); // Signal consumer that a batch is ready
    }

    // Signal that production is finished.
    std::unique_lock<std::mutex> lock(m_mutex);
    m_stop_flag = true;
    lock.unlock();
    m_cond_consumer.notify_all(); // Wake up any waiting consumers to let them exit.
}

#endif //DATALOADER_HPP
