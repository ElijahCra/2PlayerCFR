//
// Created by Elijah Crain on 8/27/23.
//
#include "Constants.hpp"
#include "Game.hpp"
#include "ConcreteGameStates.hpp"

Game::Game(std::mt19937 &engine) : mRNG(engine), mChanceProbability(1.0), mCurrentPlayer(-1), mCards(), raises(0), mUtilities()
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

