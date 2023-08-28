//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"

PreFlopChance::PreFlopChance() {

}

void PreFlopChance::transition(Game *game) {
    // Off -> Low
    game->setState(PreFlopAction::getInstance());
}

GameState& PreFlopChance::getInstance()
{
    static PreFlopChance singleton;
    return singleton;
}

void PreFlopChance::enter(Game *light) {}

void PreFlopAction::transition(Game *game) {
    // Off -> Low
    game->setState();
}

GameState& PreFlopAction::getInstance()
{
    static PreFlopAction singleton;
    return singleton;
}

/*void FlopChance::transition(Game *game) {
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
}*/
