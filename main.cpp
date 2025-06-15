#include <iostream>
#include <chrono>
#include "CFR/RegretMinimizer.hpp"

int main() {
    // Configuration options:
    // 1. Single-threaded: useAsyncStorage = false (default)
    // 2. Multi-threaded: useAsyncStorage = true
    
    std::cout << "=== Single-threaded training ===\n";
    CFR::RegretMinimizer<Preflop::Game> singleThreaded{
        std::random_device()(),  // seed
        100000,                   // cache capacity
        "./cfr_single_db",       // db path
        false                    // use sync storage
    };
    
    std::cout << "Starting single-threaded training...\n";
    singleThreaded.printStorageStats();
    
    auto start = std::chrono::high_resolution_clock::now();
    singleThreaded.Train(101000);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Single-threaded completed in " << duration.count() << " ms\n";
    singleThreaded.printStorageStats();
    
    std::cout << "\n=== Multi-threaded training ===\n";
    CFR::RegretMinimizer<Preflop::Game> multiThreaded{
        std::random_device()(),  // seed
        100000,                    // cache capacity per shard
        "./cfr_async_db",        // db path  
        true,                    // use async storage
        4                        // background threads
    };
    
    std::cout << "Starting multi-threaded training...\n";
    multiThreaded.printStorageStats();
    
    start = std::chrono::high_resolution_clock::now();
    multiThreaded.Train(101000);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Multi-threaded completed in " << duration.count() << " ms\n";
    multiThreaded.printStorageStats();
    
    return 0;
}
