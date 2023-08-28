//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAME_HPP
#define INC_2PLAYERCFR_GAME_HPP

#include "GameState.hpp"
#include <random>

class Game {
public:
    explicit Game(std::mt19937 &engine);

    inline GameState* getCurrentState() const {return mCurrentState;}

    void transition();

    void setState(GameState& newState);

private:
    GameState* mCurrentState;
    std::mt19937 mRNG;

};


#endif //INC_2PLAYERCFR_GAME_HPP
