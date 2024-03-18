#include <iostream>
#include "CFR/RegretMinimizer.hpp"



int main() {
    CFR::RegretMinimizer<Preflop::Game> Minimize{(std::random_device()())};
    Minimize.Train(101000);
}
