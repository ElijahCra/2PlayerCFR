#include <iostream>

#include "Game/Game.hpp"
#include "Game/ConcreteGameStates.hpp"
#include <random>
int main() {

    const uint32_t seed = std::random_device()();

    std::mt19937 RNG(seed);
    Game game = Game(RNG);

    Action A = Action::None;
    game.transition(A);

    std::cout << "Hello, World!" << std::endl;
}
