//
// HybridAdvantageStorage.hpp
// Thread-safe hybrid memory/disk storage for Deep CFR training samples
//

#ifndef HYBRIDADVANTAGESTORAGE_HPP
#define HYBRIDADVANTAGESTORAGE_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>
#include "torch/torch.h"
#include "types.hpp"

// Serializer for TrainingSampleAdvantage
class AdvantageSerializer {
public:
    static std::string serialize(const TrainingSampleAdvantage& sample) {
        torch::save(sample.infoset.getCardTensors(), "temp_cards.pt");
        torch::save(sample.infoset.getBetTensor(), "temp_bets.pt");

        // For simplicity, using a text format. In production, use protobuf or similar
        std::stringstream ss;
        ss << sample.iteration << "|";
        ss << sample.weight << "|";

        // Serialize legal actions
        ss << sample.legal_action_indices.size() << "|";
        for (int idx : sample.legal_action_indices) {
            ss << idx << ",";
        }
        ss << "|";

        // Serialize advantages
        for (float adv : sample.advantages) {
            ss << adv << ",";
        }

        // In production, you'd serialize tensors more efficiently
        // For now, we'll store tensor dimensions and data

        return ss.str();
    }

    static TrainingSampleAdvantage deserialize(const std::string& data) {
        // Simplified deserialization - implement based on your needs
        TrainingSampleAdvantage sample;
        // Parse the string format
        return sample;
    }
};

template<typename GameType>
class HybridAdvantageStorage {
public:
    struct Config {
        size_t in_memory_capacity = 1000000;      // 1M samples in memory
        size_t flush_batch_size = 10000;          // Flush when this many samples accumulate
        size_t max_db_size = 40000000;           // 40M samples max in DB
        std::chrono::milliseconds flush_interval = std::chrono::milliseconds(5000);
        std::string db_path = "./advantage_db";
        int num_flush_threads = 2;                // Number of background flush threads
    };

    HybridAdvantageStorage(const Config& config = Config())
        : m_config(config),
          m_in_memory_size(0),
          m_db_size(0),
          m_total_samples_seen(0),
          m_stop_flushing(false) {

        // Initialize RocksDB
        initializeDB();

        // Pre-allocate in-memory tensors similar to AdvantageMemoryBuffer
        for(int i = 0; i < GameType::NUM_CARD_TYPES; ++i) {
            auto card_shape = GameType::getCardShape(i);
            m_card_tensors.push_back(
                torch::empty({(long)m_config.in_memory_capacity, card_shape}, torch::kInt64)
            );
        }

        m_bet_tensors = torch::empty({(long)m_config.in_memory_capacity, (long)GameType::NUM_BET_FEATURES});
        m_targets = torch::empty({(long)m_config.in_memory_capacity, (long)GameType::MAX_ACTIONS});
        m_masks = torch::empty({(long)m_config.in_memory_capacity, (long)GameType::MAX_ACTIONS});
        m_weights = torch::empty({(long)m_config.in_memory_capacity, 1});

        // Start background flush threads
        for (int i = 0; i < m_config.num_flush_threads; ++i) {
            m_flush_threads.emplace_back(&HybridAdvantageStorage::flushWorker, this);
        }
    }

    ~HybridAdvantageStorage() {
        shutdown();
    }

    // Thread-safe add sample
    void add_sample(const TrainingSampleAdvantage& sample, std::mt19937& rng) {
        std::unique_lock<std::shared_mutex> lock(m_memory_mutex);

        size_t index_to_write;
        bool should_flush = false;

        if (m_in_memory_size < m_config.in_memory_capacity) {
            // Still filling the in-memory buffer
            index_to_write = m_next_idx++;
            m_in_memory_size++;

            if (m_in_memory_size % m_config.flush_batch_size == 0) {
                should_flush = true;
            }
        } else {
            // Memory buffer is full, use reservoir sampling
            std::uniform_int_distribution<size_t> dist(0, m_total_samples_seen);
            size_t sample_idx = dist(rng);

            if (sample_idx < m_config.in_memory_capacity) {
                // Replace in memory
                index_to_write = sample_idx;
            } else {
                // This sample goes directly to the flush queue
                m_flush_queue.push_back(sample);
                m_total_samples_seen++;

                if (m_flush_queue.size() >= m_config.flush_batch_size) {
                    should_flush = true;
                }

                lock.unlock();
                if (should_flush) {
                    m_flush_cv.notify_one();
                }
                return;
            }
        }

        m_total_samples_seen++;

        // Write to in-memory tensors
        writeSampleToMemory(sample, index_to_write);

        lock.unlock();

        if (should_flush) {
            m_flush_cv.notify_one();
        }
    }

    // Get batch for training (combines memory and disk if needed)
    struct BatchData {
        std::vector<torch::Tensor> cards;
        torch::Tensor bets;
        torch::Tensor targets;
        torch::Tensor masks;
        torch::Tensor weights;
        size_t batch_size;
    };

    BatchData get_batch(size_t batch_size, std::mt19937& rng) {
        std::shared_lock<std::shared_mutex> lock(m_memory_mutex);

        BatchData batch;
        batch.batch_size = std::min(batch_size, m_in_memory_size);

        if (batch.batch_size == 0) {
            return batch;  // Empty batch
        }

        // Sample indices
        std::vector<size_t> indices(m_in_memory_size);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);
        indices.resize(batch.batch_size);

        // Extract batch from in-memory tensors
        for (int i = 0; i < GameType::NUM_CARD_TYPES; ++i) {
            std::vector<torch::Tensor> card_batch;
            for (size_t idx : indices) {
                card_batch.push_back(m_card_tensors[i][idx].unsqueeze(0));
            }
            batch.cards.push_back(torch::cat(card_batch, 0));
        }

        std::vector<torch::Tensor> bet_batch, target_batch, mask_batch, weight_batch;
        for (size_t idx : indices) {
            bet_batch.push_back(m_bet_tensors[idx].unsqueeze(0));
            target_batch.push_back(m_targets[idx].unsqueeze(0));
            mask_batch.push_back(m_masks[idx].unsqueeze(0));
            weight_batch.push_back(m_weights[idx].unsqueeze(0));
        }

        batch.bets = torch::cat(bet_batch, 0);
        batch.targets = torch::cat(target_batch, 0);
        batch.masks = torch::cat(mask_batch, 0);
        batch.weights = torch::cat(weight_batch, 0);

        return batch;
    }

    // Direct tensor access for DataLoader compatibility
    const std::vector<torch::Tensor>& get_card_tensors() const {
        return m_card_tensors;
    }
    const torch::Tensor& get_bet_tensors() const {
        return m_bet_tensors;
    }
    const torch::Tensor& get_targets() const {
        return m_targets;
    }
    const torch::Tensor& get_masks() const {
        return m_masks;
    }
    const torch::Tensor& get_weights() const {
        return m_weights;
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(m_memory_mutex);
        return m_in_memory_size + m_db_size;
    }

    size_t in_memory_size() const {
        std::shared_lock<std::shared_mutex> lock(m_memory_mutex);
        return m_in_memory_size;
    }

    size_t disk_size() const {
        return m_db_size.load();
    }

    void force_flush() {
        std::unique_lock<std::shared_mutex> lock(m_memory_mutex);
        m_force_flush = true;
        lock.unlock();
        m_flush_cv.notify_all();
    }

private:
    void initializeDB() {
        rocksdb::Options options;
        options.create_if_missing = true;
        options.compression = rocksdb::kSnappyCompression;
        options.write_buffer_size = 128 * 1024 * 1024;  // 128MB
        options.max_write_buffer_number = 4;
        options.allow_concurrent_memtable_write = true;
        options.enable_write_thread_adaptive_yield = true;

        // Optimize for write throughput
        options.max_background_jobs = 8;
        options.max_background_compactions = 4;
        options.max_background_flushes = 2;

        rocksdb::DB* db;
        rocksdb::Status status = rocksdb::DB::Open(options, m_config.db_path, &db);
        if (!status.ok()) {
            throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
        }
        m_db.reset(db);

        // Get initial DB size
        std::string value;
        m_db->GetProperty("rocksdb.estimate-num-keys", &value);
        m_db_size = std::stoi(value);
    }

    void writeSampleToMemory(const TrainingSampleAdvantage& sample, size_t index) {
        // Prepare target and mask tensors
        torch::Tensor sample_target = torch::zeros({GameType::MAX_ACTIONS});
        torch::Tensor sample_mask = torch::zeros({GameType::MAX_ACTIONS});

        for (size_t i = 0; i < sample.legal_action_indices.size(); ++i) {
            int action_idx = sample.legal_action_indices[i];
            sample_target[action_idx] = sample.advantages[i];
            sample_mask[action_idx] = 1.0f;
        }

        // Write to pre-allocated tensors
        m_targets[index] = sample_target;
        m_masks[index] = sample_mask;
        m_weights[index] = sample.weight;
        m_bet_tensors[index] = sample.infoset.getBetTensor().squeeze(0);

        for(int i = 0; i < GameType::NUM_CARD_TYPES; ++i) {
            m_card_tensors[i][index] = sample.infoset.getCardTensors()[i].squeeze(0);
        }
    }

    void flushWorker() {
        while (!m_stop_flushing) {
            std::unique_lock<std::mutex> lock(m_flush_mutex);

            // Wait for work or timeout
            m_flush_cv.wait_for(lock, m_config.flush_interval, [this] {
                std::shared_lock<std::shared_mutex> mem_lock(m_memory_mutex);
                return m_stop_flushing ||
                       !m_flush_queue.empty() ||
                       m_force_flush;
            });

            if (m_stop_flushing) break;

            // Collect samples to flush
            std::vector<TrainingSampleAdvantage> to_flush;
            {
                std::unique_lock<std::shared_mutex> mem_lock(m_memory_mutex);

                if (m_force_flush || m_flush_queue.size() >= m_config.flush_batch_size) {
                    to_flush.swap(m_flush_queue);
                    m_force_flush = false;
                }
            }

            lock.unlock();

            // Flush to disk
            if (!to_flush.empty()) {
                flushToDisk(to_flush);
            }
        }
    }

    void flushToDisk(const std::vector<TrainingSampleAdvantage>& samples) {
        rocksdb::WriteBatch batch;

        for (const auto& sample : samples) {
            // Generate unique key (could use hash of infoset + timestamp)
            std::string key = std::to_string(m_total_samples_seen++) + "_" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

            std::string serialized = AdvantageSerializer::serialize(sample);
            batch.Put(key, serialized);
        }

        rocksdb::Status status = m_db->Write(rocksdb::WriteOptions(), &batch);
        if (status.ok()) {
            m_db_size += samples.size();
        } else {
            std::cerr << "Failed to flush to disk: " << status.ToString() << std::endl;
        }
    }

    void shutdown() {
        // Stop flush threads
        m_stop_flushing = true;
        m_flush_cv.notify_all();

        for (auto& thread : m_flush_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // Final flush
        force_flush();

        if (m_db) {
            m_db->Close();
        }
    }

    Config m_config;

    // In-memory storage (similar to AdvantageMemoryBuffer)
    std::vector<torch::Tensor> m_card_tensors;
    torch::Tensor m_bet_tensors;
    torch::Tensor m_targets;
    torch::Tensor m_masks;
    torch::Tensor m_weights;

    // Thread safety
    mutable std::shared_mutex m_memory_mutex;  // For in-memory buffer
    std::mutex m_flush_mutex;                  // For flush queue
    std::condition_variable m_flush_cv;

    // State tracking
    std::atomic<size_t> m_in_memory_size;
    std::atomic<size_t> m_db_size;
    std::atomic<size_t> m_total_samples_seen;
    size_t m_next_idx = 0;

    // Flush queue and control
    std::deque<TrainingSampleAdvantage> m_flush_queue;
    std::vector<std::thread> m_flush_threads;
    std::atomic<bool> m_stop_flushing;
    std::atomic<bool> m_force_flush{false};

    // RocksDB
    std::unique_ptr<rocksdb::DB> m_db;
};
