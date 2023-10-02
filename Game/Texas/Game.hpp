//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_TEXAS_GAME_HPP
#define INC_TEXAS_GAME_HPP

#include <random>
#include <array>
#include "GameBase.h"
#include "GameState.hpp"

namespace Texas {

    class Game : public GameBase {
    public:

        explicit Game(std::mt19937 &engine); //boost rng

        inline GameState *getCurrentState() const { return currentState; }

        void transition(Action action);

        void setState(GameState &newState, Action action);

        void addMoney();

        void addMoney(float amount);

        std::vector<Action> getActions() const;

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

        float getUtility(int payoffPlayer) const;

        void updateInfoSet(int player, int card);

        void updateInfoSet(Action action);

        void updatePlayer();

        std::string getInfoSet(int player) const;

        static std::string cardIntToStr(int card);

        static std::string actionToStr(Action action);

        int winner;

        void setType(std::string type);

        std::string getType() const;

        float averageUtility;

        float averageUtilitySum;

        int currentRound;



        ///@brief how many unique deals are possible
        constexpr int getChanceActionNum() const{
            if (0 == currentRound) {
                //(cardNum choose 2) * (cardNum-2 choose 2)
                return DeckCardNum * (DeckCardNum - 1) * (DeckCardNum - 2) * (DeckCardNum - 3);
            } else if (1 == currentRound) {
                return (DeckCardNum - 4) * (DeckCardNum - 5) * (DeckCardNum - 6);
            } else {
                return DeckCardNum - (currentRound + 5);
            }
        }

    private:
        std::string type;
        /// @brief the players private info set, contains their cards public cards and all actions played
        std::array <std::string, PlayerNum> infoSet{};

        /// @brief array of payoff, 1 per player final is the pot
        std::array<float, PlayerNum + 1> utilities{};

        ///@brief current gamestate i.e. preflop chance or preflopnobet
        GameState *currentState;

        ///@brief actions available at this point in the game
        std::vector <Action> availActions;


    };

}
#endif //INC_TEXAS_GAME_HPP
