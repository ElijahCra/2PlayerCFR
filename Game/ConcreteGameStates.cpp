//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>



void PreFlopChance::enter(Game *game, Action action) {
    for (int i = 0; i < CardNum; ++i) {
        game->mCards[i] = i;
    }
    // shuffle cards
    std::shuffle(game->mCards.begin(),game->mCards.end(), game->mRNG);

    //deal player cards
    for (int i = 0; i < PlayerNum; ++i) {
        game->mInfoSet[i][0] = game->mCards[2 * i];
        game->mInfoSet[i][1] = game->mCards[(2 * i) + 1];
    }
    //ante up
    game->mUtilities[0] = -0.5;
    game->mUtilities[1] = -1;
    game->mUtilities[2] = 1.5;
    std::cout << game->mInfoSet[2][0];
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
    else if (Action::Reraise == action){
        game->mUtilities[game->mCurrentPlayer] += -2;
        game->mUtilities[2] += 2;
    }
    else if (Action::Fold == action){

    }
}

void FlopChance::enter(Game *game, Action action) {}

void FlopChance::transition(Game *game, Action action) {

    game->setState(Terminal::getInstance(), Action::None);
}

GameState& FlopChance::getInstance()
{
    static FlopChance singleton;
    return singleton;
}

void FlopChance::exit(Game *game, Action action) {}


void Terminal::enter(Game *game, Action action) {}

void Terminal::transition(Game *game, Action action) {
    //Game::payoff();
}

GameState& Terminal::getInstance()
{
    static Terminal singleton;
    return singleton;
}

void Terminal::exit(Game *game, Action action) {}

