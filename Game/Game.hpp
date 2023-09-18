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
    void addMoney(double amount);

    std::vector<Action> getActions() const;

     void setActions(std::vector<Action> actionVec);

    ///@brief deck of cards
    std::array<int, CardNum> mCards;

    /// @brief acting player
    int mCurrentPlayer;

    /// @brief number of raises + reraises played this round
    uint8_t mRaises;

    ///@brief rng engine, mersienne twister
    std::mt19937& mRNG;

    ///@brief probability this node was chosen by the previous node ie probability of taking action leading to this node
    double mNodeProbability;

    double getUtility(int payoffPlayer) const;

    /// @brief public cards dealt
    std::array<int, 5> dealtCards{};


    void setInfoSet(int player, int card, int cardIndex);

    void setInfoSet(int player, Action action);
private:
    /// @brief the players private info set, contains their cards public cards and all actions played
    std::array<std::string, PlayerNum> mInfoSet{};

    /// @brief array of payoff, 1 per player final is the pot
    std::array<double, PlayerNum + 1> mUtilities{};

    ///@brief
    GameState* mCurrentState;



    ///@brief actions available at this point in the game
    std::vector<Action> mActions;
};


#endif //INC_2PLAYERCFR_GAME_HPP
