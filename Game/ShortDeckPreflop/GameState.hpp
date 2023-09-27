//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_PREFLOP_GAMESTATE_HPP
#define INC_PREFLOP_GAMESTATE_HPP

#include "Constants.hpp"
#include <vector>
#include <string>
namespace Preflop {
    class GameState {
    public:
        virtual void enter(Game *game, Action action) = 0;

        virtual void transition(Game *game, Action action) = 0;

        virtual void exit(Game *game, Action action) = 0;

        virtual ~GameState() = default;


    private:

    };
}
#endif //INC_PREFLOP_GAMESTATE_HPP
