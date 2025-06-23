#include <iostream>
#include <chrono>
#include <thread>
#include "CFR/RegretMinimizer.hpp"

int main() {
    const uint32_t iterations = 1000000;
    const uint32_t cacheSize = 100000;
    const uint32_t backgroundThreads = 2;
    const uint32_t numCFRThreads = std::thread::hardware_concurrency()-backgroundThreads;
    const uint32_t perThreadIterations = iterations / numCFRThreads;
    
    std::cout << "Hardware threads available: " << numCFRThreads << "\n\n";
    
    // =====================================================
    // Single-threaded setup with synchronous storage
    // =====================================================
    bool singleSync = false;
    if (singleSync) {
        std::cout << "=== Single-threaded CFR + Sync Storage ===\n";
        CFR::RegretMinimizer<Preflop::Game> singleThreaded{
            std::random_device()(),  // seed
            cacheSize,                  // cache capacity
            "./cfr_single_db",       // db path
            false                    // sync storage
        };

        std::cout << "Starting single-threaded training...\n";
        singleThreaded.printStorageStats();

        auto start = std::chrono::high_resolution_clock::now();
        singleThreaded.Train(numCFRThreads*iterations);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Single-threaded completed in " << duration.count() << " ms\n";
        singleThreaded.printStorageStats();
        singleThreaded.flushStorageCache();
    }

    // =====================================================
    // Single-threaded setup with Asynchronous storage
    // =====================================================
    bool singleASync = false;
    if (singleASync) {
        std::cout << "=== Single-threaded CFR + async Storage ===\n";
        CFR::RegretMinimizer<Preflop::Game> singleThreaded{
            std::random_device()(),  // seed
            cacheSize,                  // cache capacity
            "./cfr_single_db",       // db path
            true,                   // sync storage,
            backgroundThreads
        };

        std::cout << "Starting single-threaded training...\n";
        singleThreaded.printStorageStats();

        auto start = std::chrono::high_resolution_clock::now();
        singleThreaded.Train(numCFRThreads*iterations);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Single-threaded completed in " << duration.count() << " ms\n";
        singleThreaded.printStorageStats();
        singleThreaded.flushStorageCache();
    }

    // =====================================================
    // Multi-threaded CFR with async storage
    // =====================================================
    bool multiAsync = true;
    if (multiAsync) {
        std::cout << "\n=== Multi-threaded CFR + Async Storage ===\n";
        std::cout << "Starting multi-threaded CFR training with " << numCFRThreads << " threads...\n";

        auto start = std::chrono::high_resolution_clock::now();

        // Create RegretMinimizer objects with separate storage paths for each thread
        std::vector<std::unique_ptr<CFR::RegretMinimizer<Preflop::Game>>> minimizers;
        std::vector<std::thread> threads;

        for (size_t i = 0; i < numCFRThreads; ++i) {
            minimizers.emplace_back(std::make_unique<CFR::RegretMinimizer<Preflop::Game>>(
                std::random_device()(),     // unique seed per thread
                cacheSize,  // divide cache capacity among threads
                "./cfr_async_db_" + std::to_string(i),  // separate DB path per thread
                true,                       // async storage
                backgroundThreads                           // background I/O threads per minimizer
            ));

            threads.emplace_back([&minimizers, i,perThreadIterations]() {
                minimizers[i]->Train(perThreadIterations);
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }



        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Multi-threaded completed in " << duration.count() << " ms\n";
    }

    
    return 0;
}
