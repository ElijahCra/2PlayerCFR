//
// Created by elijah on 9/19/2023.
//

#include <gtest/gtest.h>
#include "../Game/Game.hpp"
#include "../CFR/RegretMin.hpp"

TEST(HelloTest, GameTests) {

    auto rng = std::mt19937(std::random_device()());
    Game* game1 = new Game(rng);
    // Expect equality.
    EXPECT_EQ(game1->getType(), "chance");

    game1->transition(Action::None);
    EXPECT_EQ(game1->getType(), "action");
    game1->transition(Action::Fold);
    EXPECT_EQ(game1->getType(),"terminal");

    auto rng2 = std::mt19937(std::random_device()());
    Game* game2 = new Game(rng2);

    game2->transition(Action::None);
    game2->transition(Action::Call);
    game2->transition(Action::Check);

    EXPECT_EQ(game2->getType(), "terminal");


}

TEST(HelloTest, CFRTests) {
    uint64_t seed = 123456;
    auto rng3 = std::mt19937(seed);
    Game* game3 = new Game(rng3);
    //game3->transition(Action::None);

    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);
    std::cout << game3->getInfoSet(0) << "\n";
    std::cout << game3->getInfoSet(1) << "\n";

    RegretMin minimizer(seed);

    double weiUtil = minimizer.ChanceCFR(*game3,0,1.0,1.0,1.0);

    EXPECT_EQ(weiUtil,3.0);
}