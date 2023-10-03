//
// Created by Elijah Crain on 8/27/23.
//

#include "Game.hpp"
#include <utility>
#include <stdexcept>
#include "ConcreteGameStates.hpp"

namespace Preflop {

    Game::Game(std::mt19937 &engine) : RNG(engine),
                                       currentPlayer(0),
                                       deckCards(),
                                       raiseNum(0),
                                       utilities(),
                                       availActions(),
                                       infoSet(),
                                       winner(-1),
                                       type("chance"),
                                       averageUtility(0),
                                       averageUtilitySum(0) {
        currentState = &ChanceState::getInstance();
        currentState->enter(this, Action::None);
    }

    void Game::setState(GameState &newState, Action action) {
        currentState->exit(this, action); //
        currentState = &newState;
        currentState->enter(this, action);
    }

    void Game::transition(Action action) {
        currentState->transition(this, action);
    }

    void Game::addMoney() { //preflop ante's
        utilities[0] = -0.5;
        utilities[1] = -1;
        utilities[2] = 1.5;
    }

    void Game::addMoney(double amount) {
        utilities[currentPlayer] -= amount;
        utilities[PlayerNum] += amount;
    }

    std::vector<GameBase::Action> Game::getActions() const {
        return availActions;
    }

    void Game::setActions(std::vector<Action> actionVec) {
        availActions = std::move(actionVec);
    }

    double Game::getUtility(int payoffPlayer) const {

        if (3 == winner) {
            return utilities[2] / 2.0 + utilities[payoffPlayer];
        } else if (payoffPlayer == winner) {
            return utilities[2] + utilities[payoffPlayer];
        } else if (-1 == winner) {
            throw std::logic_error("game winner not updated from initialization");
        } else {
            return utilities[payoffPlayer];
        }
    }

    void Game::updateInfoSet(Action action) {
        for (int i = 0; i < PlayerNum; ++i) {
            infoSet[i].append(actionToStr(action));
        }

    }

    void Game::updateInfoSet(int player, int card) {
        infoSet[player].append(cardIntToStr(card));
    }

    std::string Game::getInfoSet(int player) const {
        return infoSet[player];
    }

    std::string Game::cardIntToStr(int card) {
        if (card < 10) {
            return '0' + std::to_string(card);
        } else {
            return std::to_string(card);
        }
    }

    std::string Game::actionToStr(Action action) {
        if (Action::Check == action) {
            return {"Ch"};
        } else if (Action::Fold == action) {
            return {"Fo"};
        } else if (Action::Raise == action) {
            return {"Ra"};
        } else if (Action::Call == action) {
            return {"Ca"};
        } else if (Action::Reraise == action) {
            return {"Re"};
        } else { throw std::logic_error("cannot convert that action to str"); }

    }

    void Game::updatePlayer() {
        currentPlayer = 1 - currentPlayer;
    }

    void Game::setType(std::string type1) {
        type = std::move(type1);
    }

    std::string Game::getType() const {
        return type;
    }

    void Game::reInitialize() {
        RNG();
        currentPlayer = 0;
        raiseNum = 0;
        for (int i = 0; i < PlayerNum; ++i) {
            infoSet[i] = "";
            utilities[i] = 0;
        }
        winner = -1;
        type = "chance";
        currentState = &ChanceState::getInstance();
        currentState->enter(this, Action::None);
    }
}
