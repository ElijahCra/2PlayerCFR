//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAME_HPP
#define INC_2PLAYERCFR_GAME_HPP

#include "GameState.hpp"
#include <random>
#include <array>

namespace Preflop {

    class Game;

    enum class Action : int {
        None = -1,
        Check = 0,
        Fold = 1,
        Raise = 2,
        Call = 3,
        Reraise = 4
    };

    enum class GameStates : int {
        PreFlopChance = 0,
        PreFlopActionNoBet,
        PreFlopActionBet,
        Terminal
    };


    static constexpr int PlayerNum = 2;

    static constexpr int DeckCardNum = 13;

    static constexpr int maxRaises = 2;

///@brief how many unique deals are possible
    static constexpr int getRootChanceActionNum() {

        //(cardNum choose 2) * (cardNum-2 choose 2)
        int Actions = 1;
        Actions *= DeckCardNum * (DeckCardNum - 1) * (DeckCardNum - 2) * (DeckCardNum - 3) / 4;

        return Actions;
    }

/// @brief  player actions*roundnum + 2 private cards + 5 public cards
    static constexpr int privateInfoSetLength = (PlayerNum + maxRaises + 2) * 4 + 2 + 5;
}
    class Game {
    public:


        explicit Game(std::mt19937 &engine);

        inline Preflop::GameState *getCurrentState() const { return currentState; }

        void transition(Preflop::Action action);

        void setState(GameState &newState, Action action);

        void addMoney();

        void addMoney(double amount);

        std::vector<Action> getActions() const;

        void setActions(std::vector<Action> actionVec);

        void reInitialize();

        ///@brief deck of cards
        std::array<int, DeckCardNum> deckCards;

        /// @brief acting player
        int currentPlayer;

        /// @brief number of raises + reraises played this round
        uint8_t raiseNum;

        ///@brief rng engine, mersienne twister
        std::mt19937 &RNG;

        double getUtility(int payoffPlayer) const;

        void updateInfoSet(int player, int card);

        void updateInfoSet(Action action);

        void updatePlayer();

        std::string getInfoSet(int player) const;

        static std::string cardIntToStr(int card);

        static std::string actionToStr(Action action);

        int winner;

        void setType(std::string type);

        std::string getType() const;

        double averageUtility;

        double averageUtilitySum;

    private:
        std::string type;
        /// @brief the players private info set, contains their cards public cards and all actions played
        std::array<std::string, PlayerNum> infoSet{};

        /// @brief array of payoff, 1 per player final is the pot
        std::array<double, PlayerNum + 1> utilities{};

        ///@brief current gamestate i.e. preflop chance or preflopnobet
        GameState *currentState;

        ///@brief actions available at this point in the game
        std::vector<Action> availActions;
    };
}

#endif //INC_2PLAYERCFR_GAME_HPP
