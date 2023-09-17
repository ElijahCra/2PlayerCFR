//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include <vector>



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
    game->addMoney();
    std::cout << game->mInfoSet[2][0];

    Action availActions[1] = {Action::None};

    game->setActions(availActions);
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

std::string PreFlopChance::type() {
    return std::string{"chance"};
}



void PreFlopActionNoBet::enter(Game *game, Action action) {
    if (Action::None == action) {
        constexpr int ChanceAN = getRootChanceActionNum();
        game->mChanceProbability = 1.0 / (double) ChanceAN;

        Action availActions[3] = {Action::Call, Action::Raise,Action::Fold};
        game->setActions(availActions);

    }
    else if (Action::Call == action) {
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
        game->setState(Terminal::getInstance(), action);
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

std::string PreFlopActionNoBet::type(){
    return std::string{"action"};
}


void PreFlopActionNoBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->addMoney(0.5);
    }
    else if (Action::Raise == action) {
        game->addMoney(1.5);
        ++game->mRaises;
    }
    else if (Action::Check == action){}

    ++game->mCurrentPlayer;
}



void PreFlopActionBet::enter(Game *game, Action action) {}

void PreFlopActionBet::transition(Game *game, Action action) {
    if (Action::Call == action) { //previous player raises this player calls -> FC
        game->setState(Terminal::getInstance(), action);
    }
    else if (Action::Fold == action) { //previous player raises this player folds -> FC
        game->setState(Terminal::getInstance(), action);
    }
    else if (Action::Reraise == action) { //previous player raises we reraise -> PFB player has to decide then -> FC
        if (maxRaises < game->mRaises){
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


std::string PreFlopActionBet::type(){
    return std::string{"action"};
}


void PreFlopActionBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->addMoney(1.0);
    }
    else if (Action::Reraise == action){
        game->addMoney(2.0);
        ++game->mRaises;
    }
    else if (Action::Fold == action){}
    ++game->mCurrentPlayer;
}


void Terminal::enter(Game *game, Action action) {

}

void Terminal::transition(Game *game, Action action) {}

GameState& Terminal::getInstance()
{
    static Terminal singleton;
    return singleton;
}

std::string Terminal::type(){
    return std::string{"terminal"};
}

void Terminal::exit(Game *game, Action action) {}


/*
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
*/