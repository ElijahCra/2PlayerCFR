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

    explicit Game(std::mt19937 &engine);

    inline GameState *getCurrentState() const { return mCurrentState; }

    void transition(Action action);

    void setState(GameState &newState, Action action);

    /// @brief chance this node is chosen by previous node
    double mChanceProbability;

    std::array<int, CardNum> mCards;
/// @brief acting player
    int mCurrentPlayer;
    /// @brief number of times raise/reraise has been played this round
    uint8_t mRaises;

    std::array<std::array<uint8_t,privateInfoSetLength>, PlayerNum> mInfoSet{};
/// @brief array of payoff, 1 per player final is the pot
    std::array<double, PlayerNum + 1> mUtilities{};
private:
    GameState* mCurrentState;

    std::mt19937& mRNG;




    /// @brief public cards dealt
    std::array<int, 5> dealtCards{};
};


#endif //INC_2PLAYERCFR_GAME_HPP
