#include <iostream>

#include "CFR/RegretMin.hpp"
#include <iostream>
#include <filesystem>
#include "../Utility/Utility.hpp"


int main() {

    uint64_t seed = 1234;
    auto rng3 = std::mt19937(seed);
    Game* game3 = new Game(rng3);

    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);

    RegretMin minimizer(seed);


    std::filesystem::path cwd = std::filesystem::current_path();

    std::cout << cwd << "\n";
    std::cout << Utility::LookupSingleHands() << "\n";
}
