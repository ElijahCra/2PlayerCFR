//
// Created by Elijah Crain on 7/5/25.
//

#ifndef MULTITHREADEDTRAINER_HPP
#define MULTITHREADEDTRAINER_HPP
#include <condition_variable>
#include <memory>
#include <random>
#include <iostream>
#include <queue>

#include "RegretMinimizer.hpp"
#include "../Storage/LRUList.hpp"
#include "../Storage/ShardedLRUCache.hpp"
template<typename K, typename V> using MyMap = std::unordered_map<K, V>;

namespace CFR {
template<typename GameType, typename StorageType = ShardedLRUCache<MyMap,LRUList>>
class MultiThreadedTrainer {
public:
    explicit MultiThreadedTrainer(const uint32_t numThreads = std::thread::hardware_concurrency())
    : m_storage(std::make_shared<StorageType>()),
        m_numThreads(numThreads),
        m_totalIterationsCompleted(0),
        m_shouldStop(false),
        m_updateInterval(std::chrono::milliseconds(100)) // UI update every 100ms
       {

        m_threads.reserve(numThreads);
        m_regretMinimizers.reserve(numThreads);
        for (uint32_t i = 0; i < numThreads; ++i) {
        m_regretMinimizers.emplace_back(
            std::make_unique<RegretMinimizer<GameType, StorageType>>(
                std::random_device()(), m_storage));
        m_threads.emplace_back(&MultiThreadedTrainer::workerLoop, this, i);
    }
    }

    ~MultiThreadedTrainer() {
        stop();
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_workMutex);
            m_shouldStop = true;
        }
        m_workCondition.notify_all();

        for (auto& thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    // Train with callback for UI updates
    void TrainWithCallback(uint32_t totalIterations, std::function<void(uint32_t)> progressCallback = nullptr) {
        const uint32_t batchSize = std::max(1000u, totalIterations / (m_numThreads * 20)); // Larger batches
        const uint32_t iterationsPerThread = totalIterations / m_numThreads;

        std::cout << "Starting training with " << m_numThreads << " threads, "
                  << "batch size: " << batchSize << ", total iterations: " << totalIterations << "\n";

        m_totalIterationsCompleted = 0;

        // Queue work for each thread
        {
            std::lock_guard<std::mutex> lock(m_workMutex);
        for (uint32_t i = 0; i < m_numThreads; ++i) {
                uint32_t remaining = iterationsPerThread;
                while (remaining > 0) {
                    uint32_t batch = std::min(batchSize, remaining);
                    m_workQueue.push(batch);
                    remaining -= batch;
                }
        }
        }
        m_workCondition.notify_all();

        // Monitor progress and call callback
        auto lastUpdate = std::chrono::steady_clock::now();
        while (m_totalIterationsCompleted < totalIterations && !m_shouldStop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            auto now = std::chrono::steady_clock::now();
            if (progressCallback && (now - lastUpdate) >= m_updateInterval) {
                progressCallback(m_totalIterationsCompleted.load());
                lastUpdate = now;
            }
        }

        if (progressCallback) {
            progressCallback(totalIterations);
        }
        }

    // Legacy interface for compatibility
    void Train(uint32_t iterations) {
        TrainWithCallback(iterations);
    }

    auto getNodeInformation(const std::string& index) noexcept -> std::vector<std::vector<float>>
    {
        std::vector<std::vector<float>> res;
        auto node = m_storage->getNode(index);
        if (node)
        {
            res.push_back(node->getRegretSum());
            res.push_back(node->getStrategy());
            node->calcAverageStrategy();
            res.push_back(node->getAverageStrategy());
        }
        return res;
    }

    uint32_t getCompletedIterations() const {
        return m_totalIterationsCompleted.load();
    }


  /// @brief Set cancellation flag to interrupt training
  void setCancelled(bool cancelled) { m_shouldStop = cancelled; }

private:
    void workerLoop(uint32_t threadId) {
        while (true) {
            uint32_t workAmount = 0;

            // Wait for work
            {
                std::unique_lock<std::mutex> lock(m_workMutex);
                m_workCondition.wait(lock, [this] { return m_shouldStop || !m_workQueue.empty(); });

                if (m_shouldStop) break;

                if (!m_workQueue.empty()) {
                    workAmount = m_workQueue.front();
                    m_workQueue.pop();
                }
            }

            if (workAmount > 0) {
                // Do the actual training work
                m_regretMinimizers[threadId]->Train(workAmount);
                m_totalIterationsCompleted.fetch_add(workAmount);
            }
        }
    }

    std::shared_ptr<StorageType> m_storage;
    std::vector<std::thread> m_threads;
    std::vector<std::unique_ptr<RegretMinimizer<GameType, StorageType>>> m_regretMinimizers;
    uint32_t m_numThreads;

    // Work queue management
    std::queue<uint32_t> m_workQueue;
    std::mutex m_workMutex;
    std::condition_variable m_workCondition;
    std::atomic<bool> m_shouldStop;
    std::atomic<uint32_t> m_totalIterationsCompleted;

    // UI update timing
    std::chrono::milliseconds m_updateInterval;
};
}
#endif //MULTITHREADEDTRAINER_HPP
