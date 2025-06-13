#include <iostream>
#include "CFR/RegretMinimizer.hpp"

int main() {
    CFR::RegretMinimizer<Preflop::Game> Minimize{(std::random_device()())};
    
    std::cout << "Starting training...\n";
    Minimize.printStorageStats();
    
    Minimize.Train(1010000);
    
    std::cout << "\nAfter training:\n";
    Minimize.printStorageStats();
}
