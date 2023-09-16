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

    Action* getActions() const;

     void setActions(Action actionArray[]);

    /// @brief chance this node is chosen by previous node
    double mChanceProbability;

    ///@brief deck of cards
    std::array<int, CardNum> mCards;

    /// @brief acting player
    int mCurrentPlayer;

    /// @brief number of raises + reraises played this round
    uint8_t mRaises;

    ///@brief rng engine, mersienne twister
    std::mt19937& mRNG;

    ///@brief probability this node was chosen by the previous node ie probability of taking action leading to this node
    double nodeProbability;

private:
    /// @brief the players private info set, contains their cards public cards and all actions played
    std::array<std::array<uint8_t,privateInfoSetLength>, PlayerNum> mInfoSet{};

    /// @brief array of payoff, 1 per player final is the pot
    std::array<float, PlayerNum + 1> mUtilities{};

    ///@brief
    GameState* mCurrentState;

    /// @brief public cards dealt
    std::array<int, 5> dealtCards{};

    ///@brief actions available at this point in the game
    Action* mActions;
};


#endif //INC_2PLAYERCFR_GAME_HPP
