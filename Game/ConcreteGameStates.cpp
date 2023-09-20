//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include "../Utility/Utility.hpp"



void PreFlopChance::enter(Game *game, Action action) {
    for (int i = 1; i <+ CardNum; ++i) {
        game->mCards[i] = i;
    }
    // shuffle cards
    std::shuffle(game->mCards.begin(),game->mCards.end(), game->mRNG);

    //deal player cards
    for (int player = 0; player < PlayerNum; ++player) {
        for (int index=0; index <2; ++index) {
            game->updateInfoSet(player,game->mCards[index+2*player],index);
        }
    }
    //ante up
    game->addMoney();

    std::vector<Action> availActions {Action::None};
    game->setActions(availActions);
}

void PreFlopChance::transition(Game *game, Action action) {
    if (Action::None == action) {
        game->setState(PreFlopActionNoBet::getInstance(), Action::None);
    }
    else {throw std::logic_error("wrong action for preflopchance");}
    //std::cout << "transitioned from preflop chance \n";
}

GameState& PreFlopChance::getInstance() {
    static PreFlopChance singleton;
    return singleton;
}

void PreFlopChance::exit(Game *game, Action action) {}

std::string PreFlopChance::type() {
    return std::string{"chance"};
}



void PreFlopActionNoBet::enter(Game *game, Action action) {
    if (Action::None == action) {
        std::vector<Action> availActions {Action::Fold,Action::Raise, Action::Call};
        game->setActions(availActions);
    }
    else if (Action::Call == action) {
        std::vector<Action> availActions {Action::Check, Action::Raise};
        game->setActions(availActions);
    }
}

void PreFlopActionNoBet::transition(Game *game, Action action) {
    if (Action::Call == action) { //first action small-blind calls/limps
        game->setState(PreFlopActionNoBet::getInstance(), action);
    }
    else if (Action::Fold == action or Action::Check == action) { //first action small-blind folds
        game->setState(Terminal::getInstance(), action);          // or second action bb checks -> post flop chance node
    }
    else if (Action::Raise == action) { //first action small blind raises
        game->setState(PreFlopActionBet::getInstance(), action);
    }
    else {throw std::logic_error("wrong action for preflopnobet");}
    //std::cout << "transitioned from preflop no bet \n";
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
    game->updatePlayer();

}



void PreFlopActionBet::enter(Game *game, Action action) {
    if (Action::Raise == action) {
        game->setActions(std::vector<Action> {Action::Fold, Action::Call, Action::Reraise});
    }
    else if (Action::Reraise == action){
        if (game->mRaises >= maxRaises) {
            game->setActions(std::vector<Action>{Action::Fold, Action::Call});
        }
        else{
            game->setActions(std::vector<Action>{ Action::Fold, Action::Call,Action::Reraise});
        }
    }
}

void PreFlopActionBet::transition(Game *game, Action action) {
    if (Action::Call == action or Action::Fold == action) { //previous player raises this player calls -> FC
        game->setState(Terminal::getInstance(), action);    //or previous player raises this player folds -> FC
    }
    else if (Action::Reraise == action) { //previous player raises we reraise -> PFB player has to decide then -> FC
        if (maxRaises < game->mRaises){
            throw std::logic_error("reraised more than allowed in preflopactionbet");
        }
        game->setState(PreFlopActionBet::getInstance(), action);
    }
    else {throw std::logic_error("wrong action for preflopbet");}

    //std::cout << "transitioned from preflop bet \n";
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
        game->updatePlayer();
    }
    else if (Action::Reraise == action){
        game->addMoney(2.0);
        ++game->mRaises;
        game->updatePlayer();
    }
    else if (Action::Fold == action){}

}


void Terminal::enter(Game *game, Action action) {
    std::cout << "entering terminal\n";
    if (Action::Fold == action){
        game->winner = 1 - game->mCurrentPlayer;
        return;
    }
    game->setActions(std::vector<Action>{Action::None});

    std::array<int,7> p0cards{game->mCards[0],game->mCards[1]};
    std::array<int,7> p1cards{game->mCards[2],game->mCards[3]};

    std::copy(game->mCards.begin()+4,game->mCards.begin()+9,p0cards.begin()+2);
    std::copy(game->mCards.begin()+4,game->mCards.begin()+9,p1cards.begin()+2);


    game->winner = Utility::getWinner(p0cards.begin(),p1cards.begin());

}

void Terminal::transition(Game *game, Action action) {
     throw std::logic_error("cant transition from terminal unless??(reset?)");
}

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