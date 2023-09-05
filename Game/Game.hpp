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

    void addMoney();
    void addMoney(float amount);

    std::vector<Action> getActions();

    /// @brief chance this node is chosen by previous node
    double mChanceProbability;

    ///@brief deck of cards
    std::array<int, CardNum> mCards;

    /// @brief acting player
    int mCurrentPlayer;

    /// @brief number of raises + reraises played this round
    uint8_t mRaises;

    ///
    std::array<std::array<uint8_t,privateInfoSetLength>, PlayerNum> mInfoSet{};

    ///@brief
    std::mt19937& mRNG;

private:
    /// @brief array of payoff, 1 per player final is the pot
    std::array<float, PlayerNum + 1> mUtilities{};

    ///@brief
    GameState* mCurrentState;

    /// @brief public cards dealt
    std::array<int, 5> dealtCards{};

    ///@brief actions available at this point in the game
    std::array<int,4> mActions{-1};
};


#endif //INC_2PLAYERCFR_GAME_HPP
