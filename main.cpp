#include <iostream>
#include "CFR/RegretMinimizer.hpp"
#include "Storage/HybridNodeStorage.hpp"

int main() {
    // Option 1: Use the convenience alias
    CFR::RegretMinimizer<Preflop::Game, CFR::HybridNodeStorage<CFR::DefaultLRUCache>> Minimize;
    
    // Option 2: Or specify different containers
    // CFR::RegretMinimizer<Preflop::Game, CFR::HybridNodeStorage<CFR::LRUCache<std::map>>> Minimize;
    auto start = std::chrono::high_resolution_clock::now();
    Minimize.Train(1010000);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTraining completed in " << duration.count() << " ms\n";
}
