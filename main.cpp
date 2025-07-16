#include <iostream>

#include "CFR/MultiThreadedTrainer.hpp"
#include "CFR/RegretMinimizer.hpp"
#include "DeepCFR/DeepCFR.hpp"
#include "Evaluator/Evaluator.hpp"
#include "Evaluator/RandomStrategy.hpp"
#include "Storage/HybridNodeStorage.hpp"
#include "Storage/LRUList.hpp"
// Common convenience aliases
template<typename K, typename V> using MyMap = std::unordered_map<K, V>;
template<typename T> using MyList = std::list<T>;

int main() {
    // //CFR::RegretMinimizer<Preflop::Game, CFR::HybridNodeStorage<CFR::LRUNodeCache<MyMap,LRUList>>> Minimize; //Rocksdb and lru cach
    //
    // CFR::MultiThreadedTrainer<Preflop::Game, CFR::HybridNodeStorage<CFR::ShardedLRUCache<MyMap,LRUList>>> Minimize; //multi threaded Rocksdb and sharded lru cach
    // //CFR::RegretMinimizer<Preflop::Game> Minimize; //Raw mem cache
    // auto start = std::chrono::high_resolution_clock::now();
    // Minimize.Train(10100100);
    // auto end = std::chrono::high_resolution_clock::now();
    //
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    //
    // std::cout << "\nTraining completed in " << duration.count() << " ms\n";
    // // RandomStrategy<Preflop::Game> randomStrategy{};
    // // CFR::HybridNodeStorage<CFR::LRUNodeCache<MyMap,LRUList>> trainedStrategy{};
    // //
    // // Evaluator<Preflop::Game> evaluator;
    // // evaluator.Evaluate(trainedStrategy,randomStrategy,1000000);


    std::cout << "Starting Deep CFR Training..." << std::endl;

    // You can choose which game to train on, Preflop or Texas
    DeepRegretMinimizer<Preflop::Game> deep_cfr_minimizer;

    // Train for a specified number of iterations
    // Note: Deep CFR requires many more iterations than tabular CFR to converge.
    deep_cfr_minimizer.Train(100000);

    std::cout << "Training complete." << std::endl;
}
