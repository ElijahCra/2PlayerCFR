//
// Created by Elijah Crain on 8/27/23.
//
#include "Constants.hpp"
#include "Game.hpp"

#include <utility>
#include <stdexcept>
#include "ConcreteGameStates.hpp"
#include "../Utility/Utility.hpp"

Game::Game(std::mt19937 &engine) : mRNG(engine), mNodeProbability(1.0), mCurrentPlayer(1), mCards(), mRaises(0), mUtilities(), mActions(),
                                   mInfoSet({"00000000000000","00000000000000"}), winner(-1)
{
    mCurrentState = &PreFlopChance::getInstance();
    mCurrentState->enter(this,Action::None);
}

void Game::setState(GameState &newState, Action action) {
    mCurrentState->exit(this, action); //
    mCurrentState = &newState;
    mCurrentState->enter(this, action);
}

void Game::transition(Action action) {
    mCurrentState->transition(this, action);
}

void Game::addMoney() { //preflop ante's
    mUtilities[0] = -1;
    mUtilities[1] = -0.5;
    mUtilities[2] = 1.5;
}

void Game::addMoney(double amount) {
    mUtilities[mCurrentPlayer] -= amount;
    mUtilities[2] += amount;
}

std::vector<Action> Game::getActions() const {
    return mActions;
}

void Game::setActions(std::vector<Action> actionVec) {
    mActions = std::move(actionVec);
}

double Game::getUtility(int payoffPlayer) {

    if (2 == winner) {
        return mUtilities[2]/2.0 + mUtilities[payoffPlayer];
    }
    else if(payoffPlayer == winner) {
        return mUtilities[2] + mUtilities[payoffPlayer];
    }
    else if(-1 == winner) {
        throw std::logic_error("game winner not updated from initialization");
    }
    else {
        return mUtilities[payoffPlayer];
    }
}

void Game::updateInfoSet(Action action) {
    for (int i = 0; i < PlayerNum; ++i) {
        mInfoSet[i].append(actionToStr(action));
    }

}

void Game::updateInfoSet(int player, int card, int cardIndex) {
    mInfoSet[player].replace(cardIndex*2,2, cardIntToStr(card));
}

std::string Game::getInfoSet(int player) const{
    return mInfoSet[player];
}

std::string Game::cardIntToStr(int card) {
    if (card < 10) {
        return '0'+std::to_string(card);
    }
    else {
        return std::to_string(card);
    }
}

std::string Game::actionToStr(Action action) {
    if (Action::Check == action) {
        return {"Ch"};
    }
    else if (Action::Fold == action) {
        return {"Fo"};
    }
    else if (Action::Raise == action) {
        return {"Ra"};
    }
    else if (Action::Call == action) {
        return {"Ca"};
    }
    else if (Action::Reraise == action) {
        return {"Re"};
    }
    else {throw std::logic_error("cannot convert that action to str");}

}

void Game::updatePlayer(){
    mCurrentPlayer = 1 - mCurrentPlayer;
}

