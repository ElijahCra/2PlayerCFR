#include <iostream>

#include "Game/Game.hpp"
#include <random>
#include "Utility/Utility.hpp"

int main() {

    //uint32_t seed = std::random_device()();

    //std::mt19937 RNG(seed);

    //Game game = Game(RNG);

    //game.transition(Action::None);

    Utility::initLookup();

    int* cards = new int[7] {1,2,3,4,5,6,7};



    std::cout << Utility::LookupHand(cards) << std::endl;
}
