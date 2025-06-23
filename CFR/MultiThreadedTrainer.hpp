//
// Created by Elijah Crain on 6/22/25.
//

#ifndef MULTITHREADEDTRAINER_HPP
#define MULTITHREADEDTRAINER_HPP
#include <thread>
#include <vector>
#include <random>
#include <iostream>

#include "HybridNodeStorage.hpp"
#include "RegretMinimizer.hpp"

namespace CFR {
template<typename GameType, typename StorageType>
class MultiThreadedTrainer {
public:
    explicit MultiThreadedTrainer(const uint32_t numThreads = std::thread::hardware_concurrency())
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
            m_regretMinimizers.emplace_back(std::random_device()(), m_storage);
            m_threads.emplace_back(
                &RegretMinimizer<Preflop::Game,HybridNodeStorage<ShardedLRUCache>>::Train, &m_regretMinimizers[i],iterationsPerThread
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
    std::vector<RegretMinimizer<GameType, StorageType>> m_regretMinimizers;
    uint32_t m_numThreads;
};
}
#endif //MULTITHREADEDTRAINER_HPP
