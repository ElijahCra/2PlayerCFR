//
// Created by Elijah Crain on 8/28/23.
//

#ifndef INC_2PLAYERCFR_CONSTANTS_HPP
#define INC_2PLAYERCFR_CONSTANTS_HPP

class Game;

static constexpr int PlayerNum = 2;

static constexpr int CardNum = 7;

static constexpr int reRaises = 2;

static constexpr int getRootChanceActionNum() {
    int Actions = 1;
    for (int i = CardNum - 2 * PlayerNum + 1; i <= CardNum; ++i) {
        Actions *= Actions; // calculate CardNum_permutation_4
    }
    return Actions;
}

static constexpr int privateInfoSetLength =
        (PlayerNum + reRaises + 2) * 4 + 2 + 5; //player actions*roundnum + 2 private cards + 5 public cards


enum class Action : int {
    None = -1,
    Check,
    Fold,
    Bet,
    Call,
    Reraise,
    Num
};


#endif //INC_2PLAYERCFR_CONSTANTS_HPP
