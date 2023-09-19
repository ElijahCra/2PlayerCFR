//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAME_HPP
#define INC_2PLAYERCFR_GAME_HPP

#include "GameState.hpp"
#include <random>
#include <array>
#include <string>

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

    double getUtility(int payoffPlayer);

    void updateInfoSet(int player, int card, int cardIndex);

    void updateInfoSet(Action action);

    void updatePlayer();

    std::string getInfoSet(int player) const;

    static std::string cardIntToStr(int card);
    static std::string actionToStr(Action action);

    int winner;

private:
    /// @brief the players private info set, contains their cards public cards and all actions played
    std::array<std::string, PlayerNum> mInfoSet{};

    /// @brief array of payoff, 1 per player final is the pot
    std::array<double, PlayerNum + 1> mUtilities{};

    ///@brief current gamestate i.e. preflop chance or preflopnobet
    GameState* mCurrentState;

    ///@brief actions available at this point in the game
    std::vector<Action> mActions;
};


#endif //INC_2PLAYERCFR_GAME_HPP
