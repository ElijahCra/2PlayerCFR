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
    void exit(Game* game) override {}
    static GameState& getInstance();

private:
    PreFlopChance() = default;
    PreFlopChance(const PreFlopChance& other);
    PreFlopChance& operator=(const PreFlopChance& other);
};

class PreFlopAction : public GameState {
public:

    void enter(Game* game, Action action) override {}
    void transition(Game* game, Action action) override;
    void exit(Game* game) override {}
    static GameState& getInstance();

private:
    PreFlopAction() = default;
    PreFlopAction(const PreFlopAction& other);
    PreFlopAction& operator=(const PreFlopAction& other);
};

/*class FlopChance : public GameState {
public:
    void enter(Game* game) {}
    void transition(Game* game);
    void exit(Game* game) {}
    static GameState& getInstance();

private:
    FlopChance() {}
    FlopChance(const FlopChance& other);
    FlopChance& operator=(const FlopChance& other);
};

class FlopAction : public GameState {
public:
    void enter(Game* game) {}
    void transition(Game* game);
    void exit(Game* game) {}
    static GameState& getInstance();

private:
    FlopAction() {}
    FlopAction(const FlopAction& other);
    FlopAction& operator=(const FlopAction& other);
};

class TurnChance : public GameState {
public:
    void enter(Game* game) {}
    void transition(Game* game);
    void exit(Game* game) {}
    static GameState& getInstance();

private:
    TurnChance() {}
    TurnChance(const TurnChance& other);
    TurnChance& operator=(const TurnChance& other);
};

class TurnAction : public GameState {
public:
    void enter(Game* game) {}
    void transition(Game* game);
    void exit(Game* game) {}
    static GameState& getInstance();

private:
    TurnAction() {}
    TurnAction(const TurnAction& other);
    TurnAction& operator=(const TurnAction& other);
};

class RiverChance : public GameState {
public:
    void enter(Game* game) {}
    void transition(Game* game);
    void exit(Game* game) {}
    static GameState& getInstance();

private:
    RiverChance() {}
    RiverChance(const RiverChance& other);
    RiverChance& operator=(const RiverChance& other);
};

class RiverAction : public GameState {
public:
    void enter(Game* game) {}
    void transition(Game* game);
    void exit(Game* game) {}
    static GameState& getInstance();

private:
    RiverAction() {}
    RiverAction(const TurnChance& other);
    RiverAction& operator=(const TurnChance& other);
};

class Terminal : public GameState {
public:
    void enter(Game* game) {}
    void transition(Game* game);
    void exit(Game* game) {}
    static GameState& getInstance();

private:
    Terminal() {}
    Terminal(const Terminal& other);
    Terminal& operator=(const Terminal& other);
};*/




#endif //INC_2PLAYERCFR_CONCRETEGAMESTATES_HPP
