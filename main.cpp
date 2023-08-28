#include <iostream>

#include "Game/Game.hpp"
#include "Game/ConcreteGameStates.hpp"
int main() {

    const uint32_t seed = std::random_device()();

    auto RNG = std::mt19937(seed);

    //Game game = Game(RNG);

    //game.transition(A);
    std::cout << "Hello, World!" << std::endl;
}
