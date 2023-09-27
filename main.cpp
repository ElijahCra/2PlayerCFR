#include <iostream>
#include "CFR/RegretMinimizer.hpp"



int main() {

    RegretMinimizer<Preflop::Game> Minimize((std::random_device()()));
    Minimize.Train(10000000);
}
