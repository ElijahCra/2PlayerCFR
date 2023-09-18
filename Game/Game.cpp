//
// Created by Elijah Crain on 8/27/23.
//
#include "Constants.hpp"
#include "Game.hpp"

#include <utility>
#include "ConcreteGameStates.hpp"
#include "../Utility/Utility.hpp"

Game::Game(std::mt19937 &engine) : mRNG(engine), mNodeProbability(1.0), mCurrentPlayer(-1), mCards(), mRaises(0), mUtilities(), mActions()
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

double Game::getUtility(int payoffPlayer) const{

    int p0Cards[7];
    p0Cards[0] = stoi(mInfoSet[0].substr(0,2));
    p0Cards[1] = stoi(mInfoSet[0].substr(2,2));
    for (int i=2; i<7; ++i) {
        p0Cards[i] = dealtCards[i-2];
    }

    int p1Cards[7];
    p1Cards[0] = stoi(mInfoSet[1].substr(0,2));
    p1Cards[1] = stoi(mInfoSet[1].substr(2,2));
    for (int i=2; i<7; ++i) {
        p1Cards[i] = dealtCards[i-2];
    }

    int winner = Utility::getWinner(p0Cards, p1Cards);

    if (3 == winner) {
        return mUtilities[2]/2.0 + mUtilities[payoffPlayer];
    }
    else if(payoffPlayer == winner) {
        return mUtilities[2] + mUtilities[payoffPlayer];
    }
    else {
        return mUtilities[payoffPlayer];
    }
}

void Game::setInfoSet(int player, Action action) {

}

void Game::setInfoSet(int player, int card, int cardIndex) {

}

