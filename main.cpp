#include <iostream>
#include "CFR/RegretMinimizer.hpp"
#include "Storage/HybridNodeStorage.hpp"


int main() {
    //CFR::RegretMinimizer<Preflop::Game> Minimize{(std::random_device()())};
    CFR::RegretMinimizer<Preflop::Game, CFR::HybridNodeStorage<CFR::LRUNodeCache<std::unordered_map<std::string, typename CacheList::iterator>,std::list<CacheEntry>>>> Minimize;
    auto start = std::chrono::high_resolution_clock::now();
    Minimize.Train(1010000);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTraining completed in " << duration.count() << " ms\n";
}
