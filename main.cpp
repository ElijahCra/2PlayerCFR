#include <iostream>
#include "CFR/RegretMinimizer.hpp"

int main() {
    CFR::RegretMinimizer<Preflop::Game> Minimize{(std::random_device()())};
    
    std::cout << "Starting training...\n";
    Minimize.printStorageStats();
    Minimize.printPreflopStats();

    
    auto start = std::chrono::high_resolution_clock::now();
    Minimize.Train(101000);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTraining completed in " << duration.count() << " ms\n";

    
    std::cout << "\nAfter training:\n";
    Minimize.printStorageStats();
}
