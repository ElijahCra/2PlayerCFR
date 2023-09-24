//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include "../Utility/Utility.hpp"



void ChanceState::enter(Game *game, Action action) {
    if (DeckCardNum == 13) {
        for (int i = 0; i < DeckCardNum; ++i) {
            game->mCards[i] = 1+i*4;
        }
    } else {
        for (int i = 1; i <= DeckCardNum; ++i) {
            game->mCards[i-1] = i;
        }
    }
    game->mRNG();

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
}

void ChanceState::transition(Game *game, Action action) {
    if (Action::None == action) {
        game->setState(ActionStateNoBet::getInstance(), Action::None);
    }
    else {throw std::logic_error("wrong action for preflopchance");}
    //std::cout << "transitioned from preflop chance \n";
}

GameState& ChanceState::getInstance() {
    static ChanceState singleton;
    return singleton;
}

void ChanceState::exit(Game *game, Action action) {
    game->setActions(std::vector<Action>{Action::None});
    game->updatePlayer();

}




void ActionStateNoBet::enter(Game *game, Action action) {
    if (Action::None == action) {
        std::vector<Action> availActions {Action::Fold,Action::Raise, Action::Call};
        game->setActions(availActions);
    }
    else if (Action::Call == action) {
        std::vector<Action> availActions {Action::Check, Action::Raise};
        game->setActions(availActions);
    }
    game->setType("action");
}

void ActionStateNoBet::transition(Game *game, Action action) {
    if (Action::Call == action) { //first action small-blind calls/limps
        game->setState(ActionStateNoBet::getInstance(), action);
    }
    else if (Action::Fold == action or Action::Check == action) { //first action small-blind folds
        game->setState(TerminalState::getInstance(), action);          // or second action bb checks -> post flop chance node
    }
    else if (Action::Raise == action) { //first action small blind raises
        game->setState(ActionStateBet::getInstance(), action);
    }
    else {throw std::logic_error("wrong action for preflopnobet");}
    //std::cout << "transitioned from preflop no bet \n";
}

GameState& ActionStateNoBet::getInstance()
{
    static ActionStateNoBet singleton;
    return singleton;
}




void ActionStateNoBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->addMoney(0.5);

    }
    else if (Action::Raise == action) {
        game->addMoney(1.5);
        ++game->mRaises;
    }
    else if (Action::Check == action){}

    game->updatePlayer();
    game->updateInfoSet(action);

}



void ActionStateBet::enter(Game *game, Action action) {
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

void ActionStateBet::transition(Game *game, Action action) {
    if (Action::Call == action or Action::Fold == action) { //previous player raises this player calls -> FC
        game->setState(TerminalState::getInstance(), action);    //or previous player raises this player folds -> FC
    }
    else if (Action::Reraise == action) { //previous player raises we reraise -> PFB player has to decide then -> FC
        if (maxRaises < game->mRaises){
            throw std::logic_error("reraised more than allowed in preflopactionbet");
        }
        game->setState(ActionStateBet::getInstance(), action);
    }
    else {throw std::logic_error("wrong action for preflopbet");}

    //std::cout << "transitioned from preflop bet \n";
}

GameState& ActionStateBet::getInstance()
{
    static ActionStateBet singleton;
    return singleton;
}



void ActionStateBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->addMoney(1.0);
    }
    else if (Action::Reraise == action){
        game->addMoney(2.0);
        ++game->mRaises;
    }
    else if (Action::Fold == action){}
    game->updateInfoSet(action);
    game->updatePlayer();
}


void TerminalState::enter(Game *game, Action action) {
    game->setType("terminal");
    game->setActions(std::vector<Action>{Action::None});

    //determine winner

    if (Action::Fold == action){
        game->winner = game->mCurrentPlayer;
        return;
    }

    //std::copy(game->mCards.begin()+4,game->mCards.begin()+8,game->);
    std::array<int,7> p0cards{game->mCards[0],game->mCards[1]};
    std::array<int,7> p1cards{game->mCards[2],game->mCards[3]};

    std::copy(game->mCards.begin()+4,game->mCards.begin()+9,p0cards.begin()+2);
    std::copy(game->mCards.begin()+4,game->mCards.begin()+9,p1cards.begin()+2);

    game->winner = Utility::getWinner(p0cards.begin(),p1cards.begin());
}

void TerminalState::transition(Game *game, Action action) {
     throw std::logic_error("cant transition from terminal unless?(reset?)");
}

GameState& TerminalState::getInstance()
{
    static TerminalState singleton;
    return singleton;
}


void TerminalState::exit(Game *game, Action action) {}


/*
void FlopChance::enter(Game *game, Action action) {}

void FlopChance::transition(Game *game, Action action) {

    game->setState(TerminalState::getInstance(), Action::None);
}

GameState& FlopChance::getInstance()
{
    static FlopChance singleton;
    return singleton;
}

void FlopChance::exit(Game *game, Action action) {}
*/