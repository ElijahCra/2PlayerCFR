//
// Created by Elijah Crain on 8/28/23.
//

#ifndef INC_TEXAS_CONSTANTS_HPP
#define INC_TEXAS_CONSTANTS_HPP

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
    Chance = 0,
    ActionNoBet,
    ActionBet,
    Terminal
};

enum class Round : int {
    Preflop = 0,
    Flop,
    Turn,
    River
};


Round operator++ (Round& obj){
    obj = static_cast<Round>(static_cast<int>(obj)+1);
}


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






#endif //INC_TEXAS_CONSTANTS_HPP
