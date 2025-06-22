#include <iostream>

#include "MultiThreadedTrainer.hpp"
#include "CFR/HybridNodeStorage.hpp"
#include "CFR/RegretMinimizer.hpp"



int main() {
    int iterations = 1010000;
    //CFR::RegretMinimizer<Preflop::Game,CFR::HybridNodeStorage<CFR::ShardedLRUCache>> Minimize{(std::random_device()())};
    CFR::MultiThreadedTrainer<Preflop::Game,CFR::HybridNodeStorage<CFR::ShardedLRUCache>> MultiMinimizer{};
    auto start = std::chrono::high_resolution_clock::now();
    //Minimize.Train(iterations);
    MultiMinimizer.Train(iterations);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTraining completed in " << duration.count() << " ms\n";
}
