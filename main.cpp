#include <iostream>

#include "CFR/RegretMinimizer.hpp"
#include <iostream>
#include <filesystem>
#include "../Utility/Utility.hpp"


int main() {

    RegretMinimizer Minimize((std::random_device()()));
    Minimize.Train(10);
}
