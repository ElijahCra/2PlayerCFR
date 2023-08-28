//
// Created by Elijah Crain on 8/27/23.
//

#include "Game.hpp"
#include "ConcreteGameStates.hpp"

Game::Game(std::mt19937 &engine) : mRNG(engine)
{
    mCurrentState = &PreFlopChance::getInstance();
    mCurrentState->enter(this);
}

void Game::setState(GameState &newState) {
    mCurrentState->exit(this); //
    mCurrentState = &newState;
    mCurrentState->enter(this);
}

void Game::transition() {
    mCurrentState->transition(this);
}

