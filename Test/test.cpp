//
// Created by elijah on 9/19/2023.
//

#include <gtest/gtest.h>
#include "../Game/Game.hpp"
#include "../CFR/RegretMin.hpp"

#include <filesystem>
#include <iostream>
#include <string>

TEST(HelloTest, GameTests) {

    Utility::initLookup();

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
    Utility::initLookup();
    uint64_t seed = 1234;
    auto rng3 = std::mt19937(seed);
    Game* game3 = new Game(rng3);
    //game3->transition(Action::None);

    game3->transition(Action::None);
    game3->transition(Action::Raise);
    game3->transition(Action::Reraise);

    RegretMin minimizer(seed);

    double weiUtil = minimizer.ChanceCFR(*game3,0,1.0,1.0,1.0);

    game3->transition(Action::Call);

    double out = game3->getUtility(0);

    for (int i=0; i<CardNum;++i) {
        std::cout << game3->mCards[i];
    }
    std::cout << "\n";
    std::cout << game3->getInfoSet(0) << "\n";
    std::cout << game3->getInfoSet(1) << "\n";
    std::cout << game3->winner << "\n";


    int cards10[] = { 1, 2, 3, 4, 5, 6, 7 };
    Utility::EnumerateAll7CardHands();
    EXPECT_EQ(Utility::LookupHand(cards10),10);

    EXPECT_EQ(weiUtil,0.0);



    std::filesystem::path cwd = std::filesystem::current_path() / "filename.txt";
    std::ofstream file(cwd.string());
    file.close();


}