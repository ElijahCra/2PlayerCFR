//
// Created by Elijah Crain on 8/27/23.
//

#include "GameBase.hpp"
#include "ConcreteGameStates.hpp"
#include <utility>
#include <unordered_map>
#include <stdexcept>
#include <cassert>
#include <span>
#include <algorithm>

namespace Preflop {
Game::Game(std::mt19937 &engine) : RNG(engine),
                                   currentPlayer(0),
                                   winner(-1),
                                   type("chance"),
                                   currentRound(0),
                                   prevAction(Action::None),
                                   playerStacks({100.0f}){

  std::array<uint8_t,DeckCardNum> temp = baseDeck;
  std::shuffle(temp.begin(),temp.end(),RNG);
  std::copy(temp.begin(),temp.begin()+2*PlayerNum+5, playableCards.begin());

  addMoney();

  cards.initIndices(std::span<uint8_t, 9>(temp.begin(), 9));

  currentState = &ChanceState::getInstance();
  currentState->enter(*this, Action::None);

}

void Game::setState(GameState &newState, Action action) {
  currentState->exit(*this, action); //
  currentState = &newState;
  currentState->enter(*this, action);
}

void Game::transition(Action action) {
  auto Actions = getActions();
  assert(std::find(Actions.begin(),Actions.end(),action)!=Actions.end());
  currentState->transition(*this, action);
}

void Game::addMoney() { //preflop ante's in milliBigBlinds
  utilities[0] = -500;
  utilities[1] = -1000;
  utilities[2] = 1500;
}

void Game::addMoney(float amount) {
  utilities[currentPlayer] -= amount;
  utilities[2] += amount;
}

std::vector<GameBase::Action> Game::getActions() const noexcept{
  return availActions;
}

void Game::setActions(std::vector<GameBase::Action> actionVec) {
  availActions = std::move(actionVec);
}

float Game::getUtility(int payoffPlayer) const {

  if (3 == winner) {
    return utilities[2] / 2.f + utilities[payoffPlayer];
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

void Game::updateInfoSet() {
  // Find the end of the numeric part

  for (int j =0; j<2;++j){
    size_t i = 0;
    while (i < infoSet[j].length() && std::isdigit(infoSet[j][i])) {
      ++i;
    }
    infoSet[j] = std::to_string(cards.playerIndices[currentRound + (4 * j)]) + infoSet[j].substr(i);
  }

}

std::string Game::getInfoSet(int player) const noexcept {
  return infoSet[player];
}


std::string Game::actionToStr(Action action) {
  static std::unordered_map<Action, std::string> converter = {
      {Action::Check, "Ch"},
      {Action::Fold, "Fo"},
      {Action::Call, "Ca"},
      {Action::Raise1, "Ra1"},
      {Action::Raise2, "Ra2"},
      {Action::Raise3, "Ra3"},
      {Action::Raise5, "Ra5"},
      {Action::Raise10, "Ra10"},
      {Action::Reraise2, "Re2"},
      {Action::Reraise4, "Re4"},
      {Action::Reraise6, "Re6"},
      {Action::Reraise10, "Re10"},
      {Action::Reraise20, "Re20"},
      {Action::AllIn, "AI"}
  };
  return converter[action];
}

void Game::updatePlayer() {
  currentPlayer = 1 - currentPlayer;
}

void Game::setType(std::string type1) {
  type = std::move(type1);
}

std::string Game::getType() const noexcept{
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

  std::array<uint8_t,DeckCardNum> temp = baseDeck;
  std::shuffle(temp.begin(),temp.end(),RNG);
  std::copy(temp.begin(),temp.begin()+2*PlayerNum+5, playableCards.begin());

  cards.initIndices(std::span<uint8_t, 9>(temp.begin(), 9));

  winner = -1;
  type = "chance";
  currentRound = 0;
  currentState = &ChanceState::getInstance();
  currentState->enter(*this, Action::None);
}
}
