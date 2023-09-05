//
// Created by Elijah Crain on 8/27/23.
//
#include "Constants.hpp"
#include "Game.hpp"
#include "ConcreteGameStates.hpp"

Game::Game(std::mt19937 &engine) : mRNG(engine), mChanceProbability(1.0), mCurrentPlayer(-1), mCards(), mRaises(0), mUtilities()
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

void Game::addMoney(float amount) {
    mUtilities[mCurrentPlayer] -= amount;
    mUtilities[2] += amount;
}

std::vector<Action> Game::getActions(){
    return mCurrentState->getActions(this);
}

