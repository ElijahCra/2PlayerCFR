//
// Created by elijah on 9/19/2023.
//

#include <gtest/gtest.h>

#include "Game.hpp"
#include "Game.cpp"

#include "RegretMinimizer.hpp"

#include "hand_index.h"

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


        auto rng3 = std::mt19937(std::random_device()());
        Game *game3 = new Game(rng3);

        EXPECT_EQ(game3->currentRound, 0);
        EXPECT_EQ(game3->getType(), "chance");
        game3->transition(Game::Action::None);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Raise1);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Call);
        EXPECT_EQ(game3->getType(), "chance");

        EXPECT_EQ(game3->currentRound, 1);
        game3->transition(Game::Action::None);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Check);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Raise1);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Reraise2);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Call);
        EXPECT_EQ(game3->getType(), "chance");

        EXPECT_EQ(game3->currentRound, 2);
        game3->transition(Game::Action::None);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Raise1);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Reraise2);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Call);
        EXPECT_EQ(game3->getType(), "chance");

        EXPECT_EQ(game3->currentRound, 3);
        game3->transition(Game::Action::None);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Raise1);
        EXPECT_EQ(game3->getType(), "action");
        game3->transition(Game::Action::Fold);
        EXPECT_EQ(game3->getType(), "terminal");

        auto rng4 = std::mt19937(std::random_device()());
        Game *game4 = new Game(rng4);

        EXPECT_EQ(game4->currentRound, 0);
        EXPECT_EQ(game4->getType(), "chance");
        game4->transition(Game::Action::None);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Call);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Check);
        EXPECT_EQ(game4->getType(), "chance");

        EXPECT_EQ(game4->currentRound, 1);
        game4->transition(Game::Action::None);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Check);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Raise1);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Reraise2);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Call);
        EXPECT_EQ(game4->getType(), "chance");

        EXPECT_EQ(game4->currentRound, 2);
        game4->transition(Game::Action::None);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Check);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Check);
        EXPECT_EQ(game4->getType(), "chance");

        EXPECT_EQ(game4->currentRound, 3);
        game4->transition(Game::Action::None);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Check);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Raise1);
        EXPECT_EQ(game4->getType(), "action");
        game4->transition(Game::Action::Fold);
        EXPECT_EQ(game4->getType(), "terminal");
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

        if (windowsOS) {
            EXPECT_EQ(weiUtil, 345.214783f);
        } else {
            EXPECT_EQ(weiUtil, 1773.4375f);
        }

    }
}
    TEST(HandAbstract, MainTest) {

        hand_indexer_t preflop_indexer;
        hand_indexer_init(1, (uint8_t[]){2}, &preflop_indexer);

        hand_indexer_t flop_indexer;
        hand_indexer_init(2, (uint8_t[]){2, 3}, &flop_indexer);

        hand_indexer_t turn_indexer;
        hand_indexer_init(3, (uint8_t[]){2, 3, 1}, &turn_indexer);

        hand_indexer_t river_indexer;
        hand_indexer_init(4, (uint8_t[]){2, 3, 1, 1}, &river_indexer);

        EXPECT_EQ(hand_indexer_size(&preflop_indexer, 0) , 169);
        EXPECT_EQ(hand_indexer_size(&flop_indexer, 0) , 169);
        EXPECT_EQ(hand_indexer_size(&turn_indexer, 0) , 169);
        EXPECT_EQ(hand_indexer_size(&river_indexer, 0) , 169);
        EXPECT_EQ(hand_indexer_size(&flop_indexer, 1) , 1286792);
        EXPECT_EQ(hand_indexer_size(&turn_indexer, 1) , 1286792);
        EXPECT_EQ(hand_indexer_size(&river_indexer, 1) , 1286792);
        EXPECT_EQ(hand_indexer_size(&turn_indexer, 2) , 55190538);
        EXPECT_EQ(hand_indexer_size(&river_indexer, 2) , 55190538);
        EXPECT_EQ(hand_indexer_size(&river_indexer, 3) , 2428287420);
        EXPECT_EQ(hand_indexer_size(&river_indexer, 0) , 169);

        std::array<uint8_t,2> my_cards{};
        hand_unindex(&preflop_indexer,0,1,my_cards.data());

        EXPECT_EQ(0,my_cards[0]);
        EXPECT_EQ(5,my_cards[1]);

        hand_indexer_t my_preflop_indexer;
        hand_indexer_init(1, (uint8_t[]){2}, &my_preflop_indexer);

        hand_unindex(&my_preflop_indexer,0,98,my_cards.data());

        EXPECT_EQ(my_cards[0],4);
        EXPECT_EQ(my_cards[1],16);

        EXPECT_EQ((uint32_t)hand_index_last(&my_preflop_indexer, my_cards.data()),98);

        hand_indexer_free(&river_indexer);
        hand_indexer_free(&turn_indexer);
        hand_indexer_free(&flop_indexer);
        hand_indexer_free(&preflop_indexer);
    }

