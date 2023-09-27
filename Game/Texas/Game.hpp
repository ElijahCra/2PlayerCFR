//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_TEXAS_GAME_HPP
#define INC_TEXAS_GAME_HPP

#include <random>
#include <array>
#include "GameState.hpp"

namespace Texas {

    class Game {
    public:
        explicit Game(std::mt19937 &engine);

        inline GameState *getCurrentState() const { return currentState; }

        void transition(Action action);

        void setState(GameState &newState, Action action);

        void addMoney();

        void addMoney(double amount);

        std::vector <Action> getActions() const;

        void setActions(std::vector <Action> actionVec);

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

        Round currentRound;

    private:
        std::string type;
        /// @brief the players private info set, contains their cards public cards and all actions played
        std::array <std::string, PlayerNum> infoSet{};

        /// @brief array of payoff, 1 per player final is the pot
        std::array<double, PlayerNum + 1> utilities{};

        ///@brief current gamestate i.e. preflop chance or preflopnobet
        GameState *currentState;

        ///@brief actions available at this point in the game
        std::vector <Action> availActions;


    };

}
#endif //INC_TEXAS_GAME_HPP
