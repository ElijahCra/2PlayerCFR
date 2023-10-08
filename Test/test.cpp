//
// Created by elijah on 9/19/2023.
//

#include <gtest/gtest.h>
//#include <benchmark/benchmark.h>
#include "../Game/Texas/Game.hpp"
#include "../Game/Texas/Game.cpp"

#include "../CFR/RegretMinimizer.hpp"


#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>

//using wsl on my windows machine so detect linux header
#ifdef __linux__
    #define windowsOS true
#else
    #define windowsOS false
#endif


namespace Texas {
    TEST(GameTests, Game1) {

        auto rng = std::mt19937(std::random_device()());
        Game *game1 = new Game(rng);
        // Expect equality.
        EXPECT_EQ(game1->getType(), "chance");
        game1->transition(Game::Action::None);
        EXPECT_EQ(game1->getType(), "action");
        game1->transition(Game::Action::Fold);
        EXPECT_EQ(game1->getType(), "terminal");

    }

    TEST(GameTests, Game2) {
        auto rng2 = std::mt19937(std::random_device()());
        Game *game2 = new Game(rng2);

        EXPECT_EQ(game2->currentRound, 0);
        EXPECT_EQ(game2->getType(), "chance");
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Call);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);

        EXPECT_EQ(game2->currentRound, 1);
        EXPECT_EQ(game2->getType(), "chance");
        game2->transition(Game::Action::None);\
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);

        EXPECT_EQ(game2->currentRound, 2);
        EXPECT_EQ(game2->getType(), "chance");
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);

        EXPECT_EQ(game2->currentRound, 3);
        EXPECT_EQ(game2->getType(), "chance");
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "terminal");
    }

    TEST(GameTests, Game3) {
        auto rng2 = std::mt19937(std::random_device()());
        Game *game2 = new Game(rng2);

        EXPECT_EQ(game2->currentRound, 0);
        EXPECT_EQ(game2->getType(), "chance");
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Raise1);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Call);
        EXPECT_EQ(game2->getType(), "chance");

        EXPECT_EQ(game2->currentRound, 1);
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Raise1);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Reraise2);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Call);
        EXPECT_EQ(game2->getType(), "chance");

        EXPECT_EQ(game2->currentRound, 2);
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Raise1);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Reraise2);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Call);
        EXPECT_EQ(game2->getType(), "chance");

        EXPECT_EQ(game2->currentRound, 3);
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Raise1);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Fold);
        EXPECT_EQ(game2->getType(), "terminal");
    }

    TEST(GameTests, Game4) {
        auto rng2 = std::mt19937(std::random_device()());
        Game *game2 = new Game(rng2);

        EXPECT_EQ(game2->currentRound, 0);
        EXPECT_EQ(game2->getType(), "chance");
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Call);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "chance");

        EXPECT_EQ(game2->currentRound, 1);
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Raise1);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Reraise2);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Call);
        EXPECT_EQ(game2->getType(), "chance");

        EXPECT_EQ(game2->currentRound, 2);
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "chance");

        EXPECT_EQ(game2->currentRound, 3);
        game2->transition(Game::Action::None);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Check);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Raise1);
        EXPECT_EQ(game2->getType(), "action");
        game2->transition(Game::Action::Fold);
        EXPECT_EQ(game2->getType(), "terminal");
    }

    TEST(UtilityTests, WorkingTest) {
        Utility::initLookup();
        int cards10[] = {1, 2, 3, 4, 5, 6, 7};
        //Utility::EnumerateAll7CardHands();
        int val1 = Utility::LookupSingleHands();
        EXPECT_EQ(val1, 4145);
        int val2 = Utility::LookupHand(cards10);
        EXPECT_EQ(val2, 32769);

        int card4[7] = {8, 9, 3, 4, 5, 6, 7};
        int val4 = Utility::LookupHand(card4);
        EXPECT_EQ(val4 >> 12, 8);

        int card5[7] = {1, 5, 9, 13, 17, 21, 25};
        int val5 = Utility::LookupHand(card5);
        int card6[7] = {9, 13, 17, 21, 25, 29, 33};
        int val6 = Utility::LookupHand(card6);
        EXPECT_GT(val6, val5);

    }


    TEST(RegretMinTests, Test1) {
        //Utility::initLookup();
        uint64_t seed = 120;
        auto rng3 = std::mt19937(seed);
        Game *game3 = new Game(rng3);
        //game3->transition(Action::None);

        game3->transition(Game::Action::None);
        game3->transition(Game::Action::Raise1);
        game3->transition(Game::Action::Reraise2);

        CFR::RegretMinimizer<Game> minimizer(seed);

        float weiUtil = minimizer.ChanceCFR(*game3, 1, 1.0, 1.0);

        game3->transition(Game::Action::Call);

        int polook[7] = {game3->deckCards[0], game3->deckCards[1], 0, 0, 0, 0, 0};
        int p1look[7] = {game3->deckCards[2], game3->deckCards[3], 0, 0, 0, 0, 0};
        for (int i = 4; i < 9; ++i) {
            polook[i - 2] = game3->deckCards[i];
            p1look[i - 2] = game3->deckCards[i];
        }

        if (windowsOS){
            EXPECT_EQ(weiUtil, 345.214783f);
        } else {
            EXPECT_EQ(weiUtil, 1.7734375f);
        }

    }
}