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
    Game* game = new Game(rng);
    // Expect equality.
    EXPECT_EQ(game->getCurrentState()->type(), "chance");

    game->transition(Action::None);
    EXPECT_EQ(game->getCurrentState()->type(), "action");

    EXPECT_ANY_THROW(game->transition(Action::None));

}