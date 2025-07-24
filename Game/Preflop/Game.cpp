//
// Created by Elijah Crain on 8/27/23.
//

#include "GameBase.hpp"
#include "ConcreteGameStates.hpp"
#include "Game.hpp"

#include <utility>
#include <unordered_map>
#include <stdexcept>
#include <cassert>
#include <span>
#include <algorithm>
#include <format>

namespace Preflop {
Game::Game(std::mt19937 &engine) :
    RNG(engine),
    bettingSequence(positions_per_round*NUM_CARD_TYPES,-1.0f)
  {
  std::array<uint8_t,DeckCardNum> temp = baseDeck;
  std::ranges::shuffle(temp.begin(),temp.end(),RNG);
  std::copy(temp.begin(),temp.begin()+2*PlayerNum+5, playableCards.begin());

  addMoney();

  cards.initIndices(std::span<uint8_t, 9>(temp.begin(), 9));
  initCardTensors(std::span<uint8_t, 9>(temp.begin(), 9));

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

void Game::recordAction(Action action) {
      switch(action) {
          case Action::Check:
              break;
          case Action::Fold:
              break;
      case Action::Call:
            if (prevAction == Action::None)
            {
             bettingSequence[currentBettingPosition] = 500;
            } else
            {
              bettingSequence[currentBettingPosition] = bettingSequence[currentBettingPosition-1];
            }
          case Action::Raise1:
              bettingSequence[currentBettingPosition] = 1000;
          case Action::Raise2:
              // Record the actual bet/call amount
              bettingSequence[currentBettingPosition] = 2000;
              break;
          default: break;
      }
      currentBettingPosition++;
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
  assert(-1 != winner);
  if (3 == winner) {
    return utilities[2] / 2.f + utilities[payoffPlayer];
  } else if (payoffPlayer == winner) {
    return utilities[2] + utilities[payoffPlayer];
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
    infoSet[j] = std::format("{}{}", cards.playerIndices[currentRound + (2 * j)], infoSet[j].substr(i));
  }
}

std::string Game::getInfoSet(int player) const noexcept {
  return infoSet[player];
}

std::string Game::actionToStr(Action action) {
  using enum Preflop::GameBase::Action;
  static std::unordered_map<Action, std::string> converter = {
      {Check, "Ch"},
      {Fold, "Fo"},
      {Call, "Ca"},
      {Raise1, "Ra1"},
      {Raise2, "Ra2"},
      {Raise3, "Ra3"},
      {Raise5, "Ra5"},
      {Raise10, "Ra10"},
      {Reraise2, "Re2"},
      {Reraise4, "Re4"},
      {Reraise6, "Re6"},
      {Reraise10, "Re10"},
      {Reraise20, "Re20"},
      {AllIn, "AI"}
  };
  return converter[action];
}

void Game::updateCurrentPlayer() {
  currentPlayer = 1 - currentPlayer;
}

void Game::setType(std::string type1) {
  type = std::move(type1);
}

std::string Game::getType() const noexcept{
  return type;
}

void Game::initCardTensors(std::span<uint8_t, 9> cards)
{
    m_cardTensors[0].clear();
    m_cardTensors[1].clear();
    for (int p=0; p<2;++p) { //2 players
        m_cardTensors[p].push_back(torch::from_blob(cards.data()+(2*p),{1,2}, torch::kUInt8).to(torch::kInt)); // each player's hole cards
        m_cardTensors[p].push_back(torch::from_blob(cards.data()+4,{1,3}, torch::kUInt8).to(torch::kInt)); // flop
        m_cardTensors[p].push_back(torch::from_blob(cards.data()+7,{1,1}, torch::kUInt8).to(torch::kInt)); // turn
        m_cardTensors[p].push_back(torch::from_blob(cards.data()+8,{1,1}, torch::kUInt8).to(torch::kInt)); // river
    }
    // std::cout << m_cardTensors[0].size() << std::endl;
    // std::cout << m_cardTensors[0][0].sizes() << std::endl;
    // std::cout << m_cardTensors[0][1].sizes() << std::endl;
    // std::cout << m_cardTensors[0][2].sizes() << std::endl;
    // std::cout << m_cardTensors[0][3].sizes() << std::endl;
    // std::cout <<m_cardTensors[0][0] << std::endl;
}

std::vector<torch::Tensor> Game::getCardTensors(int player, int round) const noexcept
{
    std::vector<torch::Tensor> cardTensors;
    for (int i=0; i < round+1; ++i) {
        cardTensors.push_back(m_cardTensors[player][i].clone());
    }
    for (int i = round+1; i<4; ++i)
    {
      if (i==1)
      {
        cardTensors.push_back(torch::zeros({1,3}).to(torch::kInt));
      } else if (i ==2)
      {
        cardTensors.push_back(torch::zeros({1,1}).to(torch::kInt));
      } else if (i ==3)
      {
        cardTensors.push_back(torch::zeros({1,1}).to(torch::kInt));
      }
    }
    return std::move(cardTensors);
}

torch::Tensor Game::getBetTensor() const noexcept {
    torch::Tensor betTensor = torch::zeros({1, total_bet_positions });

    for (int i = 0; i < bettingSequence.size(); ++i) {
        if (bettingSequence[i] > 0) {
            betTensor[0][i] = bettingSequence[i];                // Bet amount
        }
        // If 0, bet amount remains 0 (and the network will derive bet_occurred from this)
    }

    return betTensor;
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
  std::ranges::shuffle(temp.begin(),temp.end(),RNG);
  std::copy(temp.begin(),temp.begin()+2*PlayerNum+5, playableCardsBegin());

  cards.initIndices(std::span<uint8_t, 9>(temp.begin(), 9));
  initCardTensors(std::span<uint8_t, 9>(temp.begin(), 9));

  winner = -1;
  type = "chance";
  currentRound = 0;
  currentState = &ChanceState::getInstance();
  currentState->enter(*this, Action::None);

  std::ranges::fill(bettingSequence,-1.0f);
}

void Game::updateAverageUtilitySum(float value) {
  averageUtilitySum += value;
}

void Game::updateAverageUtility(int i) {
  averageUtility = averageUtilitySum / ((float)i);
}

float Game::getAverageUtility() const noexcept {
  return averageUtility;
}

int Game::getCurrentPlayer() const noexcept{
  return currentPlayer;
}
int Game::getPlayableCards(int index) const noexcept {
  return playableCards[index];
}
std::array<unsigned char, 9>::iterator Game::playableCardsBegin() {
  return playableCards.begin();
}
}
