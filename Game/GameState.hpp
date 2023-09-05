//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAMESTATE_HPP
#define INC_2PLAYERCFR_GAMESTATE_HPP

#include "Constants.hpp"
#include <vector>

class GameState{
public:
    virtual void enter(Game* game, Action action) = 0;
    virtual void transition(Game* game, Action action) = 0;
    virtual void exit(Game* game, Action action) = 0;
    virtual ~GameState() = default;
    virtual std::vector<Action> getActions(Game* game) = 0;
private:

};

#endif //INC_2PLAYERCFR_GAMESTATE_HPP
