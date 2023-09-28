//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAMESTATE_HPP
#define INC_2PLAYERCFR_GAMESTATE_HPP


#include <vector>
#include <string>


namespace Preflop {

    /// constants
    static constexpr int PlayerNum = 2;

    static constexpr int DeckCardNum = 13;

    static constexpr int maxRaises = 2;

    enum class Action : int {
        None = -1,
        Check = 0,
        Fold = 1,
        Raise = 2,
        Call = 3,
        Reraise = 4
    };

    enum class Round : int {
        Preflop = 0,
        Flop,
        Turn,
        River
    };

    Round  operator++(Round& obj){
        obj = static_cast<Round> (static_cast<int> (obj) +1);
        return obj;
    }

    class Game;

    class GameState {
    public:
        virtual void enter(Game *game, Action action) = 0;

        virtual void transition(Game *game, Action action) = 0;

        virtual void exit(Game *game, Action action) = 0;

        virtual ~GameState() = default;

    private:

    };
}
#endif //INC_2PLAYERCFR_GAMESTATE_HPP
