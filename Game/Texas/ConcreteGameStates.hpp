//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_TEXAS_CONCRETEGAMESTATES_HPP
#define INC_TEXAS_CONCRETEGAMESTATES_HPP

#include "Game.hpp"


namespace Texas {

    class ChanceState : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();


    private:
        ChanceState() = default;

        ChanceState(const ChanceState &copy);

        ChanceState &operator=(const ChanceState &copy);
    };


    class ActionStateNoBet : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        ActionStateNoBet() = default;

        ActionStateNoBet(const ActionStateNoBet &copy);

        ActionStateNoBet &operator=(const ActionStateNoBet &copy);
    };


    class ActionStateBet : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        ActionStateBet() = default;

        ActionStateBet(const ActionStateBet &copy);

        ActionStateBet &operator=(const ActionStateBet &copy);
    };


    class TerminalState : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        TerminalState() = default;

        TerminalState(const TerminalState &copy);

        TerminalState &operator=(const TerminalState &copy);
    };


    class TreeChanceState : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        TreeChanceState() = default;

        TreeChanceState(const TreeChanceState &copy);

        TreeChanceState &operator=(const TreeChanceState& copy);
    };

    class TreeActionNoBetState : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        TreeActionNoBetState() = default;

        TreeActionNoBetState(const TreeActionNoBetState &copy);

        TreeActionNoBetState &operator=(const TreeActionNoBetState& copy);
    };

    class TreeActionBetState : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        TreeActionBetState() = default;

        TreeActionBetState(const TreeActionBetState &copy);

        TreeActionBetState &operator=(const TreeActionBetState& copy);
    };

    class TreeTerminalState : public GameState {
    public:
        void enter(Game &game, GameBase::Action action) override;

        void transition(Game &game, GameBase::Action action) override;

        void exit(Game &game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        TreeTerminalState() = default;

        TreeTerminalState(const TreeTerminalState &copy);

        TreeTerminalState &operator=(const TreeTerminalState& copy);
    };


}
#endif //INC_TEXAS_CONCRETEGAMESTATES_HPP
