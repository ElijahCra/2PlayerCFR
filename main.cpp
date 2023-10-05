#include <iostream>
#include "CFR/RegretMinimizer.hpp"



int main() {
    CFR::RegretMinimizer<Texas::Game> Minimize{(std::random_device()())};
    Minimize.Train(10000000);
}
