//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>

void PreFlopChance::transition(Game *game, Action action) {
    game->setState(PreFlopAction::getInstance(), Action::None);
    std::cout << "transitioned from preflop chance \n";
}

GameState& PreFlopChance::getInstance()
{
    static PreFlopChance singleton;
    return singleton;
}

void PreFlopChance::enter(Game *game, Action action) {
    constexpr int ChanceAN = getRootChanceActionNum();
    game->mChanceProbability = 1.0 / (double) ChanceAN;
    for (int i = 0; i < CardNum; ++i) {
        game->mCards[i] = i;
    }
    // shuffle cards
    int a = (int) action;
    for (int c1 = (int) game->mCards.size() - 1; c1 > 0; --c1) {
        const int c2 = a % (c1 + 1);
        const int tmp = game->mCards[c1];
        game->mCards[c1] = game->mCards[c2];
        game->mCards[c2] = tmp;
        a = (int) a / (c1 + 1);
    }
    //deal player cards
    for (int i = 0; i < PlayerNum; ++i) {
        game->mInfoSet[i][0] = game->mCards[2 * i];
        game->mInfoSet[i][1] = game->mCards[(2 * i) + 1];
    }
    std::cout << game->mInfoSet[1][0];
}

void PreFlopChance::exit(Game *game, Action action) {
    ++game->mCurrentPlayer;
}

void PreFlopAction::transition(Game *game, Action action) {
    if (Action::Check == action) { //p1 call small blind previous
        game->setState(FlopChance)

        game->setState(PreFlopAction::getInstance(), Action::None);
    }
    else if(game->raises >= reRaises){

    }
}

GameState& PreFlopAction::getInstance()
{
    static PreFlopAction singleton;
    return singleton;
}

void FlopChance::transition(Game *game) {
    // Off -> Low
    game->setState(FlopAction::getInstance());
}

GameState& FlopChance::getInstance()
{
    static FlopChance singleton;
    return singleton;
}


void FlopAction::transition(Game *game) {
    // Off -> Low
    game->setState();
}

GameState& FlopAction::getInstance()
{
    static FlopAction singleton;
    return singleton;
}

void TurnChance::transition(Game *game) {
    // Off -> Low
    game->setState(TurnAction::getInstance());
}

GameState& TurnChance::getInstance()
{
    static TurnChance singleton;
    return singleton;
}

void TurnAction::transition(Game *game) {
    // Off -> Low
    game->setState();
}

GameState& TurnAction::getInstance()
{
    static TurnAction singleton;
    return singleton;
}

void RiverChance::transition(Game *game) {
    // Off -> Low
    game->setState(RiverChance::getInstance());
}

GameState& RiverChance::getInstance()
{
    static RiverChance singleton;
    return singleton;
}

void RiverAction::transition(Game *game) {

    if () {

    }
    game->setState(Terminal::getInstance());
}

GameState& RiverAction::getInstance()
{
    static RiverAction singleton;
    return singleton;
}

void Terminal::transition(Game *game) {
    Game::payoff();
}

GameState& Terminal::getInstance()
{
    static Terminal singleton;
    return singleton;
}

