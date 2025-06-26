#include <iostream>

#include "MultiThreadedTrainer.hpp"
#include "Storage/HybridNodeStorage.hpp"
#include "CFR/RegretMinimizer.hpp"
#define GLOG_EXPORT
int main() {
    int iterations = 10000000;
    //CFR::RegretMinimizer<Preflop::Game,CFR::HybridNodeStorage<CFR::ShardedLRUCache>> Minimize{(std::random_device()())};
    CFR::MultiThreadedTrainer<Preflop::Game,CFR::HybridNodeStorage<CFR::ShardedLRUCache>> MultiMinimizer{};
    auto start = std::chrono::high_resolution_clock::now();
    //Minimize.Train(iterations);
    MultiMinimizer.Train(iterations);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTraining completed in " << duration.count() << " ms\n";
}
