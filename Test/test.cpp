//
// Created by elijah on 9/19/2023.
//

#include <gtest/gtest.h>
#include "../Game/Game.hpp"
#include "../CFR/RegretMin.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>

TEST(GameTests, Game1) {

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
TEST(GameTests, Game2) {
    auto rng2 = std::mt19937(std::random_device()());
    Game* game2 = new Game(rng2);

    game2->transition(Action::None);
    game2->transition(Action::Call);
    game2->transition(Action::Check);

    EXPECT_EQ(game2->getType(), "terminal");
}

TEST(UtilityTests, WorkingTest) {
    Utility::initLookup();
    int cards10[] = { 1, 2, 3, 4, 5, 6, 7 };
    //Utility::EnumerateAll7CardHands();
    int val1 = Utility::LookupSingleHands();
    EXPECT_EQ(val1,4145);
    int val2 = Utility::LookupHand(cards10);
    EXPECT_EQ(val2,32769);

    int card3[7]= {1,2,3,4,5,6,7};
    int val3 = Utility::LookupHand(card3);
    int card4[7]= {8,9,3,4,5,6,7};
    int val4 = Utility::LookupHand(card4);
    EXPECT_GT(val4,val3);

    int card5[7]= {1,2,3,4,5,6,7};
    int val5 = Utility::LookupHand(card5);
    int card6[7]= {1,2,3,4,5,8,9};
    int val6 = Utility::LookupHand(card6);
    EXPECT_GT(val5,val6);

}



TEST(RegretMinTests, Test1) {
    //Utility::initLookup();
    uint64_t seed = 12345;
    auto rng3 = std::mt19937(seed);
    Game* game3 = new Game(rng3);
    //game3->transition(Action::None);

    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);

    RegretMin minimizer(seed);

    double weiUtil = minimizer.ChanceCFR(*game3,0,1.0,1.0,1.0);

    game3->transition(Action::Call);

    double out = game3->getUtility(1);
    std::cout << out << "\n";

    for (int i=0; i<CardNum;++i) {
        std::cout << game3->mCards[i];
    }
    std::cout << "\n";
    std::cout << game3->getInfoSet(0) << "\n";
    std::cout << game3->getInfoSet(1) << "\n";
    std::cout << game3->winner << "\n";





    EXPECT_EQ(weiUtil,-1.5);


}