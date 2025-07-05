//
// Created by Elijah Crain on 7/5/25.
//

#ifndef MULTITHREADEDTRAINER_HPP
#define MULTITHREADEDTRAINER_HPP
#include <memory>
#include <random>

#include "RegretMinimizer.hpp"
#include "../Storage/LRUList.hpp"
#include "../Storage/ShardedLRUCache.hpp"
template<typename K, typename V> using MyMap = std::unordered_map<K, V>;

namespace CFR {
template<typename GameType, typename StorageType = ShardedLRUCache<MyMap,LRUList>>
class MultiThreadedTrainer {
public:
    explicit MultiThreadedTrainer(const uint32_t numThreads = 2)//std::thread::hardware_concurrency())
    : m_storage(std::make_shared<StorageType>()), m_numThreads(numThreads)
    {
        m_threads.reserve(numThreads);
        m_regretMinimizers.reserve(numThreads);
    }

    void Train(const uint32_t iterations)
    {
        const uint32_t iterationsPerThread = iterations / m_numThreads;

        std::cout << "Starting training with " << m_numThreads << " threads, "
                  << iterationsPerThread << " iterations per thread\n";

        for (uint32_t i = 0; i < m_numThreads; ++i) {
            m_regretMinimizers.emplace_back(std::make_unique<RegretMinimizer<GameType, StorageType>>(std::random_device()(), m_storage));
            m_threads.emplace_back(
                &RegretMinimizer<GameType,StorageType>::Train, m_regretMinimizers[i].get(),iterationsPerThread
            );
        }

        for (uint32_t i = 0; i < m_numThreads; ++i) {
            m_threads[i].join();
        }
        m_threads.clear();
        m_regretMinimizers.clear();
    }
private:
    std::shared_ptr<StorageType> m_storage;
    std::vector<std::thread> m_threads;
    std::vector<std::unique_ptr<RegretMinimizer<GameType, StorageType>>> m_regretMinimizers;
    uint32_t m_numThreads;
};
}
#endif //MULTITHREADEDTRAINER_HPP
