#include <iostream>
#include "CFR/RegretMinimizer.hpp"
#include "Storage/HybridNodeStorage.hpp"
#include "Storage/LRUList.hpp"
// Common convenience aliases
template<typename K, typename V> using MyMap = std::unordered_map<K, V>;
template<typename T> using MyList = std::list<T>;

int main() {
    //CFR::RegretMinimizer<Preflop::Game, CFR::HybridNodeStorage<CFR::LRUNodeCache<MyMap,LRUList>>> Minimize; //Rocksdb and lru cache
    
    CFR::RegretMinimizer<Preflop::Game> Minimize; //Raw mem cache
    auto start = std::chrono::high_resolution_clock::now();
    Minimize.Train(1010000);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTraining completed in " << duration.count() << " ms\n";
}
