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
    //Utility::EnumerateAll7CardHands();

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
    EXPECT_EQ(val4>>12,8);

    int card5[7]= {1,2,3,4,5,6,7};
    int val5 = Utility::LookupHand(card5);
    int card6[7]= {1,2,3,4,5,8,9};
    int val6 = Utility::LookupHand(card6);
    EXPECT_GT(val5,val6);

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

    RegretMin minimizer(seed);

    double weiUtil = minimizer.ChanceCFR(*game3,1,1.0,1.0,1.0);

    game3->transition(Action::Call);
    /*std::cout << game3->winner << "\n";

    int polook[7] = {game3->mCards[0],game3->mCards[1],0,0,0,0,0};
    int p1look[7] = {game3->mCards[2],game3->mCards[3],0,0,0,0,0};
    for (int i=4; i<CardNum;++i) {
        polook[i-2] = game3->mCards[i];
        p1look[i-2] = game3->mCards[i];
    }
    for (int i=0; i<7;++i) {
        std::cout << polook[i];

    }
    std::cout <<"\n";
    for (int i=0; i<7;++i) {
        std::cout << p1look[i];

    }
    std::cout <<"\n";

    std::cout << std::to_string(Utility::LookupHand(polook))<<"\n";
    std::cout << std::to_string(Utility::LookupHand(p1look))<<"\n";
    std::cout << std::to_string(Utility::getWinner(polook,p1look))<<"\n";
    */
    EXPECT_EQ(weiUtil,1.5);


}