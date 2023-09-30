//
// Created by elijah on 9/19/2023.
//

#include <gtest/gtest.h>
#include "../Game/Texas/Game.hpp"
#include "../Game/Preflop/Game.hpp"
#include "../CFR/RegretMinimizer.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>

using Game = Texas::Game;
using Action = Texas::Game::Action;
TEST(TexasGameTests, BasicTest) {

    Utility::initLookup();

    auto rng = std::mt19937(std::random_device()());
    Game *game1 = new Game(rng);
    // Expect equality.
    EXPECT_EQ(game1->getType(), "chance");

    game1->transition(Action::None);
    EXPECT_EQ(game1->getType(), "action");
    game1->transition(Action::Fold);
    EXPECT_EQ(game1->getType(), "terminal");

}

TEST(TexasGameTests, ChecksAlltheWay) {
    auto rng2 = std::mt19937(0);
    Game *game2 = new Game(rng2);

    EXPECT_EQ(game2->currentRound, 0);
    game2->transition(Action::None);
    game2->transition(Action::Call);
    game2->transition(Action::Check);

    EXPECT_EQ(game2->currentRound, 1);
    game2->transition(Action::None);
    game2->transition(Action::Check);
    game2->transition(Action::Check);

    EXPECT_EQ(game2->currentRound, 2);
    game2->transition(Action::None);
    game2->transition(Action::Check);
    EXPECT_EQ(game2->currentRound, 2);
    game2->transition(Action::Check);
    EXPECT_EQ(game2->currentRound, 3);

    game2->transition(Action::None);
    game2->transition(Action::Check);
    game2->transition(Action::Check);

    EXPECT_EQ(game2->getType(), "terminal");
    EXPECT_EQ(game2->getUtility(0),-1);
}

TEST(TexasGameTests, RaisesAlltheWay) {
    auto rng3 = std::mt19937(0);
    Game *game3 = new Game(rng3);

    EXPECT_EQ(game3->currentRound, 0);
    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);
    game3->transition(Action::Call);

    EXPECT_EQ(game3->currentRound, 1);
    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);
    game3->transition(Action::Call);

    EXPECT_EQ(game3->currentRound, 2);
    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);
    game3->transition(Action::Call);
    EXPECT_EQ(game3->currentRound, 3);

    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);
    game3->transition(Action::Call);

    EXPECT_EQ(game3->getType(), "terminal");
    EXPECT_ANY_THROW(game3->transition(Action::Call));
    EXPECT_EQ(game3->getUtility(1),9);
}

/*
TEST(UtilityTests, WorkingTest) {
    Utility::initLookup();
    int cards10[] = { 1, 2, 3, 4, 5, 6, 7 };
    //Utility::EnumerateAll7CardHands();
    int val1 = Utility::LookupSingleHands();
    EXPECT_EQ(val1,4145);
    int val2 = Utility::LookupHand(cards10);
    EXPECT_EQ(val2,32769);

    int card4[7]= {8,9,3,4,5,6,7};
    int val4 = Utility::LookupHand(card4);
    EXPECT_EQ(val4>>12,8);

    int card5[7]= {1,5,9,13,17,21,25};
    int val5 = Utility::LookupHand(card5);
    int card6[7]= {9,13,17,21,25,29,33};
    int val6 = Utility::LookupHand(card6);
    EXPECT_GT(val6,val5);

}



TEST(RegretMinTests, Test1) {
    //Utility::initLookup();
    uint64_t seed = 120;
    auto rng3 = std::mt19937(seed);
    Game* game3 = new Game(rng3);
    //game3->transition(Action::None);

    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);

    CFR::RegretMinimizer<Texas::Game> minimizer(seed);

    double weiUtil = minimizer.ChanceCFR(*game3,1,1.0,1.0);

    game3->transition(Action::Call);

    int polook[7] = {game3->deckCards[0], game3->deckCards[1], 0, 0, 0, 0, 0};
    int p1look[7] = {game3->deckCards[2], game3->deckCards[3], 0, 0, 0, 0, 0};
    for (int i=4; i < 9; ++i) {
        polook[i-2] = game3->deckCards[i];
        p1look[i-2] = game3->deckCards[i];
    }


    EXPECT_EQ(weiUtil,0.5);
*/