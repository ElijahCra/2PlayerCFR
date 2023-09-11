//
// Created by Elijah Crain on 8/28/23.
//

#ifndef INC_2PLAYERCFR_CONSTANTS_HPP
#define INC_2PLAYERCFR_CONSTANTS_HPP

class Game;

enum class Action : int {
    None = -1,
    Check,
    Fold,
    Raise,
    Call,
    Reraise,
    Num
};

enum class GameStates : int {
    PreFlopChance = 0,
    PreFlopActionNoBet,
    PreFlopActionBet,
    Terminal
};


static constexpr int PlayerNum = 2;

static constexpr int CardNum = 7;

static constexpr int maxRaises = 2;

static constexpr int getRootChanceActionNum() {

    //cardNum choose 2 * cardNum-2 choose 2

    if (CardNum == 7) {
        int Actions = 36*21;
        return Actions;
    } else {
        int Actions = 1;
        for (int i = CardNum - (2 * PlayerNum + 1); i <= CardNum; ++i) {
            Actions *= Actions; // calculate CardNum_permutation_4
        }
    return Actions;
    }
}

static constexpr int privateInfoSetLength =
        (PlayerNum + maxRaises + 2) * 4 + 2 + 5; //player actions*roundnum + 2 private cards + 5 public cards





#endif //INC_2PLAYERCFR_CONSTANTS_HPP
