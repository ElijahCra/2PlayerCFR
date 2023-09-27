//
// Created by Elijah Crain on 8/28/23.
//

#ifndef INC_2PLAYERCFR_CONSTANTS_HPP
#define INC_2PLAYERCFR_CONSTANTS_HPP

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




#endif //INC_2PLAYERCFR_CONSTANTS_HPP
