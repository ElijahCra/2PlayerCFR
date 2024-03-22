//
// Created by Elijah Crain on 3/15/24.
//


#include <gtest/gtest.h>

#include "../../Game/GameImpl/Preflop/Game.hpp"
#include "../../Game/GameImpl/Preflop/Game.cpp"

#include "RegretMinimizer.hpp"


//using wsl on my windows machine so detect linux header
#ifdef __MINGW32__
#define windowsOS true
#else
#define windowsOS false
#endif


namespace Preflop {
TEST(PreflopTests, Game1) {
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
  game2->transition(Game::Action::None);
  EXPECT_EQ(game2->getType(), "action");
  game2->transition(Game::Action::Check);
  EXPECT_EQ(game2->getType(), "action");
  game2->transition(Game::Action::Check);

  EXPECT_EQ(game2->currentRound, 2);
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
  EXPECT_EQ(game3->currentRound, 2);
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
  EXPECT_EQ(game4->currentRound, 2);
  EXPECT_EQ(game4->getType(), "terminal");

}

TEST(PreflopUtilityTests, WorkingTest) {
  Utility::initLookup();
  int cards10[] = {1, 2, 3, 4, 5, 6, 7};
  //Utility::EnumerateAll7CardHands();
  //int val1 = Utility::LookupSingleHands();
  //EXPECT_EQ(val1, 4145);
  int val2 = Utility::LookupHandValue(cards10);
  EXPECT_EQ(val2, 32769);

  int card4[7] = {8, 9, 3, 4, 5, 6, 7};
  int val4 = Utility::LookupHandValue(card4);
  EXPECT_EQ(val4 >> 12, 8);

  int card5[7] = {1, 5, 9, 13, 17, 21, 25};
  int val5 = Utility::LookupHandValue(card5);
  int card6[7] = {9, 13, 17, 21, 25, 29, 33};
  int val6 = Utility::LookupHandValue(card6);
  EXPECT_GT(val6, val5);
}


TEST(PreflopRegretMinTests, Test1) {
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

  int polook[7] = {game3->getPlayableCards(0), game3->getPlayableCards(1), 0, 0, 0, 0, 0};
  int p1look[7] = {game3->getPlayableCards(2), game3->getPlayableCards(3), 0, 0, 0, 0, 0};
  for (int i = 4; i < 9; ++i) {
    polook[i - 2] = game3->getPlayableCards(i);
    p1look[i - 2] = game3->getPlayableCards(i);
  }

  if (windowsOS) {
    EXPECT_EQ(weiUtil, -218.75f);
  } else {
    EXPECT_EQ(weiUtil, 2250.f);
  }

}
TEST(PreflopHandAbstract, MainTest) {
  uint8_t cards1[] ={2};
  uint8_t cards2[] ={2,5};

  hand_indexer_t preflop_indexer;
  hand_indexer_init(1, cards1, &preflop_indexer);

  hand_indexer_t flop_indexer;
  hand_indexer_init(2, cards2, &flop_indexer);

  EXPECT_EQ(hand_indexer_size(&preflop_indexer, 0) , 169);
  EXPECT_EQ(hand_indexer_size(&flop_indexer, 0) , 169);

  uint8_t testCards1[] = {0,1};
  EXPECT_EQ(hand_indexer_size(&flop_indexer, 1) , 123156254);



  std::array<uint8_t,2> my_cards{};

  /*hand_unindex(&preflop_indexer,0,1,my_cards.data());
  EXPECT_EQ(0,my_cards[0]);
  EXPECT_EQ(5,my_cards[1]);*/

  hand_indexer_t my_preflop_indexer;
  hand_indexer_init(1, cards1, &my_preflop_indexer);
  hand_unindex(&my_preflop_indexer,0,98,my_cards.data());

  hand_indexer_t my_flop_indexer;
  hand_indexer_init(1, cards2, &my_flop_indexer);

  EXPECT_EQ(my_cards[0],4);
  EXPECT_EQ(my_cards[1],16);

  EXPECT_EQ((uint32_t)hand_index_last(&my_preflop_indexer, my_cards.data()),98);
  //EXPECT_EQ((uint32_t)hand_index_last(&my_flop_indexer,))

  hand_indexer_free(&flop_indexer);
  hand_indexer_free(&preflop_indexer);
}
TEST(PreflopHandAbstract, 0123EqualTest) {
  uint8_t cards1[] ={2};
  uint8_t cards2[] ={2,5};

  hand_indexer_t preflop_indexer;
  hand_indexer_init(1, cards1, &preflop_indexer);

  hand_indexer_t flop_indexer;
  hand_indexer_init(2, cards2, &flop_indexer);

  hand_indexer_state_t hand1indeces;
  const uint8_t cardsp0[]{0, 1};
  hand_indexer_state_init(&flop_indexer, &hand1indeces);
  uint64_t index1 = hand_index_next_round(&flop_indexer, cardsp0, &hand1indeces);

  hand_indexer_state_t hand2indeces;
  const uint8_t cardsp1[]{2, 3};
  hand_indexer_state_init(&flop_indexer, &hand2indeces);
  uint64_t index2 = hand_index_next_round(&flop_indexer, cardsp1, &hand2indeces);
  EXPECT_EQ(index1,index2);
}
TEST(PreflopHandAbstract, 0415EqualTest) {
  uint8_t cards1[] ={2};
  uint8_t cards2[] ={2,5};

  hand_indexer_t preflop_indexer;
  hand_indexer_init(1, cards1, &preflop_indexer);

  hand_indexer_t flop_indexer;
  hand_indexer_init(2, cards2, &flop_indexer);

  hand_indexer_state_t hand1indeces;
  const uint8_t cardsp0[]{0, 4};
  hand_indexer_state_init(&flop_indexer, &hand1indeces);
  uint64_t index1 = hand_index_next_round(&flop_indexer, cardsp0, &hand1indeces);

  hand_indexer_state_t hand2indeces;
  const uint8_t cardsp1[]{1, 5};
  hand_indexer_state_init(&flop_indexer, &hand2indeces);
  uint64_t index2 = hand_index_next_round(&flop_indexer, cardsp1, &hand2indeces);
  EXPECT_EQ(index1,index2);
}
TEST(PreflopHandAbstract, 0437EqualTest) {
  uint8_t cards1[] ={2};
  uint8_t cards2[] ={2,5};

  hand_indexer_t preflop_indexer;
  hand_indexer_init(1, cards1, &preflop_indexer);

  hand_indexer_t flop_indexer;
  hand_indexer_init(2, cards2, &flop_indexer);

  hand_indexer_state_t hand1indeces;
  const uint8_t cardsp0[]{0, 4};
  hand_indexer_state_init(&flop_indexer, &hand1indeces);
  uint64_t index1 = hand_index_next_round(&flop_indexer, cardsp0, &hand1indeces);

  hand_indexer_state_t hand2indeces;
  const uint8_t cardsp1[]{3, 7};
  hand_indexer_state_init(&flop_indexer, &hand2indeces);
  uint64_t index2 = hand_index_next_round(&flop_indexer, cardsp1, &hand2indeces);
  EXPECT_EQ(index1,index2);
}

TEST(PreflopHandAbstract, 1234NotEqualTest) {
  uint8_t cards1[] ={2};
  uint8_t cards2[] ={2,5};

  hand_indexer_t preflop_indexer;
  hand_indexer_init(1, cards1, &preflop_indexer);

  hand_indexer_t flop_indexer;
  hand_indexer_init(2, cards2, &flop_indexer);

  hand_indexer_state_t hand1indeces;
  const uint8_t cardsp0[]{1, 2};
  hand_indexer_state_init(&flop_indexer, &hand1indeces);
  uint64_t index1 = hand_index_next_round(&flop_indexer, cardsp0, &hand1indeces);

  hand_indexer_state_t hand2indeces;
  const uint8_t cardsp1[]{3, 4};
  hand_indexer_state_init(&flop_indexer, &hand2indeces);
  uint64_t index2 = hand_index_next_round(&flop_indexer, cardsp1, &hand2indeces);
  EXPECT_NE(index1,index2);
}

TEST(PreflopHandAbstract, UnindexTest) {
  uint8_t cards1[] ={2};
  uint8_t cards2[] ={2,5};

  uint8_t canonicalCards[]{};
  hand_indexer_t preflop_indexer;
  hand_indexer_init(1, cards1, &preflop_indexer);

  hand_unindex(&preflop_indexer,0,0,canonicalCards);
  EXPECT_EQ(canonicalCards[0],0);
  EXPECT_EQ(canonicalCards[1],1);

  hand_unindex(&preflop_indexer,0,90,canonicalCards);
  EXPECT_EQ(canonicalCards[0],48);
  EXPECT_EQ(canonicalCards[1],49);

  hand_unindex(&preflop_indexer,0,1,canonicalCards);
  EXPECT_EQ(canonicalCards[0],0);
  EXPECT_EQ(canonicalCards[1],5);

  hand_unindex(&preflop_indexer,0,2,canonicalCards);
  EXPECT_EQ(canonicalCards[0],4);
  EXPECT_EQ(canonicalCards[1],5);

  hand_unindex(&preflop_indexer,0,3,canonicalCards);
  EXPECT_EQ(canonicalCards[0],8);
  EXPECT_EQ(canonicalCards[1],1);

  hand_unindex(&preflop_indexer,0,4,canonicalCards);
  EXPECT_EQ(canonicalCards[0],8);
  EXPECT_EQ(canonicalCards[1],5);

  std::array<std::string,13> labels {"A","K","Q","J","T","9","8","7","6","5","4","3","2"};
  std::array<std::pair<unsigned int,unsigned int>,169> indecs;
  for (int i = 0; i < 13; ++i) {
    for (int j=0; j<13; ++j) {
      hand_unindex(&preflop_indexer,0,13*i+j,canonicalCards);
      indecs[13*i+j] = {canonicalCards[0],canonicalCards[1]};
    }
  }

  std::array<std::array<uint32_t,13>,13> mapper = { 90, 168, 167,166,165,164,163,162,161,160,159,158,157,
                                                89,77,156,155,154,153,152,151,150,149,148,147,146,
                                                           88,76,65,145,144,143,142,141,140,139,138,137,136,
                                                           87,75,64,54,135,134,133,132,131,130,129,128,127,
                                                           86,74,63,53,44,126,125,124,123,122,121,120,119,
                                                           85,73,62,52,43,35,118,117,116,115,114,113,112,
                                                           84,72,61,51,42,34,27,111,110,109,108,107,106,
                                                           83,71,60,50,41,33,26,20,105,104,103,102,101,
                                                           82,70,59,49,40,32,25,19,14,100,99,98,97,
                                                           81,69,58,48,39,31,24,18,13,9,96,95,94,
                                                           80,68,57,47,38,30,23,17,12,8,5,93,92,
                                                           79,67,56,46,37,29,22,16,11,7,4,2,91,
                                                           78,66,55,45,36,28,21,15,10,6,3,1,0
  };
  EXPECT_EQ(mapper[0][0],90);

  hand_unindex(&preflop_indexer,0,4,canonicalCards);
  EXPECT_EQ(canonicalCards[0],8);
  EXPECT_EQ(canonicalCards[1],5);

}

}

