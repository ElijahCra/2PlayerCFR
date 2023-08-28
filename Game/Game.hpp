//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAME_HPP
#define INC_2PLAYERCFR_GAME_HPP

#include "GameState.hpp"
#include <random>
#include <array>

class Game {
public:
    //constants
    static constexpr int PlayerNum = 2;
    static constexpr int CardNum = 7;
    static constexpr int reRaises = 2;
    static constexpr int getRootChanceActionNum() {
        int Actions = 1;
        for (int i = CardNum - 2 * PlayerNum + 1; i <= CardNum; ++i) {
            Actions *= Actions; // calculate CardNum_permutation_4
        }
        return Actions;
    }
    static constexpr int privateInfoSetLength =
            (PlayerNum + reRaises + 2) * 4 + 2 + 5; //player actions*roundnum + 2 private cards + 5 public cards
    //end constants

    explicit Game(std::mt19937 &engine);

    inline GameState *getCurrentState() const { return mCurrentState; }

    void transition();

    void setState(GameState &newState);

    /// @brief chance this node is chosen by previous node
    double mChanceProbability;

    std::array<int, CardNum> mCards;

    int mCurrentPlayer;
    /// @brief number of times raise/reraise has been played this round
    uint8_t raises;

    std::array<std::array<uint8_t,privateInfoSetLength>, PlayerNum> mInfoSet{};

private:
    GameState* mCurrentState;

    std::mt19937& mRNG;

    /// @brief array of payoff, 1 per player final is the pot
    std::array<double, PlayerNum + 1> mUtilities;
    /// @brief acting player

    /// @brief public cards dealt
    std::array<int, 5> dealtCards{};






};


#endif //INC_2PLAYERCFR_GAME_HPP
