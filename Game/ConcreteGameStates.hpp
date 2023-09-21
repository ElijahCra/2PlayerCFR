//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_CONCRETEGAMESTATES_HPP
#define INC_2PLAYERCFR_CONCRETEGAMESTATES_HPP

#include "GameState.hpp"
#include "Game.hpp"



class PreFlopChance : public GameState {
public:
    void enter(Game* game, Action action) override;
    void transition(Game* game, Action action) override;
    void exit(Game* game, Action action) override;
    static GameState& getInstance();

private:
    PreFlopChance() = default;
    PreFlopChance(const PreFlopChance& other);
    PreFlopChance& operator=(const PreFlopChance& other);
};



class PreFlopActionNoBet : public GameState {
public:
    void enter(Game* game, Action action) override;
    void transition(Game* game, Action action) override;
    void exit(Game* game, Action action) override;
    static GameState& getInstance();
    std::string type();

private:
    PreFlopActionNoBet() = default;
    PreFlopActionNoBet(const PreFlopActionNoBet& other);
    PreFlopActionNoBet& operator=(const PreFlopActionNoBet& other);
};



class PreFlopActionBet : public GameState {
public:
    void enter(Game* game, Action action) override;
    void transition(Game* game, Action action) override;
    void exit(Game* game, Action action) override;
    static GameState& getInstance();
    std::string type();

private:
    PreFlopActionBet() = default;
    PreFlopActionBet(const PreFlopActionBet& other);
    PreFlopActionBet& operator=(const PreFlopActionBet& other);
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

class Terminal : public GameState {
public:
    void enter(Game* game, Action action) override;
    void transition(Game* game, Action action) override;
    void exit(Game* game, Action action) override;
    static GameState& getInstance();
    std::string type();

private:
    Terminal() = default;
    Terminal(const Terminal& other);
    Terminal& operator=(const Terminal& other);
};


#endif //INC_2PLAYERCFR_CONCRETEGAMESTATES_HPP
