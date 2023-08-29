//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>



void PreFlopChance::enter(Game *game, Action action) {

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
    //ante up
    game->mUtilities[0] = -0.5;
    game->mUtilities[1] = -1;
    game->mUtilities[2] = 1.5;
    std::cout << game->mInfoSet[1][0];
}

void PreFlopChance::transition(Game *game, Action action) {
    game->setState(PreFlopActionNoBet::getInstance(), Action::None);
    std::cout << "transitioned from preflop chance \n";
}

GameState& PreFlopChance::getInstance() {
    static PreFlopChance singleton;
    return singleton;
}

void PreFlopChance::exit(Game *game, Action action) {
    ++game->mCurrentPlayer;
}



void PreFlopActionNoBet::enter(Game *game, Action action) {
    if (Action::None == action) {
        constexpr int ChanceAN = getRootChanceActionNum();
        game->mChanceProbability = 1.0 / (double) ChanceAN;
    }
    else if (Action::Call == action) {
        //todo
        game->mChanceProbability = 0;
    }
}
void PreFlopActionNoBet::transition(Game *game, Action action) {
    if (Action::Call == action) { //first action small-blind calls/limps
        game->setState(PreFlopActionNoBet::getInstance(), action);
    }
    else if (Action::Fold == action) { //first action small-blind folds
        game->setState(Terminal::getInstance(), action);
    }
    else if (Action::Check == action) { //second action bb checks -> post flop chance node
        game->setState(FlopChance::getInstance(), action);
    }
    else if (Action::Raise == action) { //first action small blind raises
        game->setState(PreFlopActionBet::getInstance(), action);
    }
    else {throw std::logic_error("wrong action for preflopnobet");}
}

GameState& PreFlopActionNoBet::getInstance()
{
    static PreFlopActionNoBet singleton;
    return singleton;
}

void PreFlopActionNoBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->mUtilities[0] += -0.5;
        game->mUtilities[2] += 0.5;
    }
    else if (Action::Raise == action) {
        game->mUtilities[game->mCurrentPlayer] += -1.5;
        game->mUtilities[2] += 1.5;
    }
    else if (Action::Check == action){}

}



void PreFlopActionBet::enter(Game *game, Action action) {}

void PreFlopActionBet::transition(Game *game, Action action) {
    if (Action::Call == action) { //previous player raises this player calls -> FC
        game->setState(FlopChance::getInstance(), action);
    }
    else if (Action::Fold == action) { //previous player raises this player folds -> FC
        game->setState(Terminal::getInstance(), action);
    }
    else if (Action::Reraise == action) { //previous player raises we reraise -> PFB player has to decide then -> FC
        if (maxRaises <= game->mRaises){
            throw std::logic_error("reraised more than allowed in preflopactionbet");
        }
        game->setState(PreFlopActionBet::getInstance(), action);
    }
    else {throw std::logic_error("wrong action for preflopbet");}
}

GameState& PreFlopActionBet::getInstance()
{
    static PreFlopActionBet singleton;
    return singleton;
}

void PreFlopActionBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->mUtilities[game->mCurrentPlayer] += -1;
        game->mUtilities[2] += 1;
    }
    else if (Action::Check == action){

    }
}



void FlopChance::transition(Game *game) {

    //game->setState(FlopAction::getInstance());
}

GameState& FlopChance::getInstance()
{
    static FlopChance singleton;
    return singleton;
}


void FlopAction::transition(Game *game) {

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

