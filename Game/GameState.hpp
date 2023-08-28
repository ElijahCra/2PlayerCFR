//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAMESTATE_HPP
#define INC_2PLAYERCFR_GAMESTATE_HPP

class Game;

class GameState{
public:
    virtual void enter(Game* game) = 0;
    virtual void transition(Game* game) = 0;
    virtual void exit(Game* game) = 0;
    virtual ~GameState() = default;
};

#endif //INC_2PLAYERCFR_GAMESTATE_HPP
