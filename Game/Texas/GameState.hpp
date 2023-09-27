//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_TEXAS_GAMESTATE_HPP
#define INC_TEXAS_GAMESTATE_HPP

#include "Constants.hpp"
#include <vector>
#include <string>

class GameState{
public:
    virtual void enter(Game* game, Action action) = 0;
    virtual void transition(Game* game, Action action) = 0;
    virtual void exit(Game* game, Action action) = 0;
    virtual ~GameState() = default;

private:

};

#endif //INC_TEXAS_GAMESTATE_HPP
