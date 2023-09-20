//
// Created by elijah on 9/19/2023.
//

#include <gtest/gtest.h>
#include "../Game/Game.hpp"
#include "../CFR/RegretMin.hpp"

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {
    // Expect two strings not to be equal.
    EXPECT_STRNE("hello", "world");
    // Expect equality.
    EXPECT_EQ(7 * 6, 42);
}

TEST(HelloTest, GameTests) {

    auto rng = std::mt19937(std::random_device()());
    Game* game1 = new Game(rng);
    // Expect equality.
    EXPECT_EQ(game1->getCurrentState()->type(), "chance");

    game1->transition(Action::None);
    EXPECT_EQ(game1->getCurrentState()->type(), "action");
    game1->transition(Action::Fold);
    EXPECT_EQ(game1->getCurrentState()->type(),"terminal");

    auto rng2 = std::mt19937(std::random_device()());
    Game* game2 = new Game(rng2);

    game2->transition(Action::None);
    game2->transition(Action::Call);
    game2->transition(Action::Check);

    EXPECT_EQ(game1->getCurrentState()->type(), "terminal");

    auto seed = std::random_device()();
    auto rng3 = std::mt19937(std::random_device()());
    Game* game3 = new Game(rng3);
    game3->transition(Action::None);
    std::cout << game3->getInfoSet(1);
    //game3->transition(Action::Raise);
    //game3->transition(Action::Reraise);

    RegretMin minimizer(seed);

    double weiUtil = minimizer.ChanceCFR(*game3,0,1.0,1.0,1.0);

    EXPECT_EQ(weiUtil,3.0);
}