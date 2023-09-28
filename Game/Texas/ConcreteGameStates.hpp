//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_TEXAS_CONCRETEGAMESTATES_HPP
#define INC_TEXAS_CONCRETEGAMESTATES_HPP

#include "Game.hpp"


namespace Texas {

    class ChanceState : public GameState {
    public:
        void enter(Game *game, GameBase::Action action) override;

        void transition(Game *game, GameBase::Action action) override;

        void exit(Game *game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        ChanceState() = default;

        ChanceState(const ChanceState &other);

        ChanceState &operator=(const ChanceState &other);
    };


    class ActionStateNoBet : public GameState {
    public:
        void enter(Game *game, GameBase::Action action) override;

        void transition(Game *game, GameBase::Action action) override;

        void exit(Game *game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        ActionStateNoBet() = default;

        ActionStateNoBet(const ActionStateNoBet &other);

        ActionStateNoBet &operator=(const ActionStateNoBet &other);
    };


    class ActionStateBet : public GameState {
    public:
        void enter(Game *game, GameBase::Action action) override;

        void transition(Game *game, GameBase::Action action) override;

        void exit(Game *game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        ActionStateBet() = default;

        ActionStateBet(const ActionStateBet &other);

        ActionStateBet &operator=(const ActionStateBet &other);
    };


    class TerminalState : public GameState {
    public:
        void enter(Game *game, GameBase::Action action) override;

        void transition(Game *game, GameBase::Action action) override;

        void exit(Game *game, GameBase::Action action) override;

        static GameState &getInstance();

    private:
        TerminalState() = default;

        TerminalState(const TerminalState &other);

        TerminalState &operator=(const TerminalState &other);
    };

/*
class FlopChance : public GameState {
public:
    void enter(Game* game, Action action);
    void transition(Game* game, Action action);
    void exit(Game* game, Action action);
    static GameState& getInstance();

private:
    FlopChance() = default;
    FlopChance(const FlopChance& other);
    FlopChance& operator=(const FlopChance& other);
};

class PostFlopActionP0NoBet : public GameState {
public:
    void enter(Game* game, Action action);
    void transition(Game* game, Action action);
    void exit(Game* game, Action action);
    static GameState& getInstance();
private:
    PostFlopActionP0NoBet() = default;
    PostFlopActionP0NoBet(const PostFlopActionP0NoBet& other);
    PostFlopActionP0NoBet& operator=(const PostFlopActionP0NoBet& other);
};


class PostFlopActionP0Bet : public GameState {
public:
    void enter(Game* game, Action action) override;
    void transition(Game* game, Action action) override;
    void exit(Game* game, Action action) override;
    static GameState& getInstance();
private:
    PostFlopActionP0Bet() = default;
    PostFlopActionP0Bet(const PostFlopActionP0Bet& other);
    PostFlopActionP0Bet& operator=(const PostFlopActionP0Bet& other);
};


class PostFlopActionP1Bet : public GameState {
public:
    void enter(Game* game, Action action);
    void transition(Game* game, Action action);
    void exit(Game* game, Action action);
    static GameState& getInstance();
private:
    PostFlopActionP1Bet() = default;
    PostFlopActionP1Bet(const PostFlopActionP1Bet& other);
    PostFlopActionP1Bet& operator=(const PostFlopActionP1Bet& other);
};


class PostFlopActionP1NoBet : public GameState {
public:
    void enter(Game* game, Action action);
    void transition(Game* game, Action action);
    void exit(Game* game, Action action);
    static GameState& getInstance();
private:
    PostFlopActionP1NoBet() = default;
    PostFlopActionP1NoBet(const PostFlopActionP1NoBet& other);
    PostFlopActionP1NoBet& operator=(const PostFlopActionP1NoBet& other);
};*/
}
#endif //INC_TEXAS_CONCRETEGAMESTATES_HPP
