#include <iostream>

#include "CFR/RegretMin.hpp"
#include <iostream>


int main() {

    RegretMin minimizer;
    minimizer.Train(10000);
    std::cout << "nice";
}
