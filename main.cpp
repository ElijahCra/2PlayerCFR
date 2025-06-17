#include <iostream>
#include "CFR/RegretMinimizer.hpp"



int main() {
    CFR::RegretMinimizer<Preflop::Game> Minimize{(std::random_device()())};
    auto start = std::chrono::high_resolution_clock::now();
    Minimize.Train(1010000);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nTraining completed in " << duration.count() << " ms\n";
}
