//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_REGRETMINIMIZER_HPP
#define INC_2PLAYERCFR_REGRETMINIMIZER_HPP

#include <iostream>
#include <thread>
#include <random>
#include "../Game/GameImpl/Preflop/Game.hpp"
#include "../Game/GameImpl/Texas/Game.hpp"
#include <unordered_map>
#include "Node.hpp"
#include "../Game/Utility/Utility.hpp"
#include "CustomExceptions.h"

namespace CFR {

template<typename GameType>
class RegretMinimizer {
 public:
  /// @brief constructor takes a seed or one is generated
  explicit RegretMinimizer(uint32_t seed = std::random_device()());
  RegretMinimizer(RegretMinimizer& other) = delete;
  auto operator=(RegretMinimizer& other) -> RegretMinimizer& = delete;
  ~RegretMinimizer();

  /// @brief calls cfr algorithm for full game tree (or sampled based on version) traversal the specified number of times
  void Train(int iterations);

  [[nodiscard]]
  auto getNodeInformation(const std::string& index) noexcept -> std::vector<std::vector<float>>;


  /// @brief recursively traverse game tree (depth-first) sampling only one chance outcome at each chance node and all actions
  /// @param updatePlayer player whose getStrategy is updated and utilities are retrieved in terms of
  /// @param probCounterFactual probability of reaching the next node given all players actions and chance except for the updateCurrentPlayer's actions
  /// @param probUpdatePlayer probability of reaching next node given only the updateCurrentPlayer's actions
  /// @param game the next node in the dfs
  /// probCounterfactual does not include chance probabilities bc we divivide by the probability the chance outcomes were sampled making them 1 for the calculation
  auto ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) -> float;

  /// @brief same as ChanceCFR except at each action node sample one action for non update player
  auto ExternalSamplingCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) -> float;


 private:
  std::mt19937 rng;

  [[no_unique_address]] Utility util;

  std::unordered_map<std::string, Node *> nodeMap;

  GameType Game;

  uint64_t nodesTouched{};

};


///Implementation of templates above
template<typename GameType>
RegretMinimizer<GameType>::RegretMinimizer(const uint32_t seed) : rng(seed), Game(rng) {}

template<typename GameType>
RegretMinimizer<GameType>::~RegretMinimizer() {
  for (auto &itr: nodeMap) {
    delete itr.second;
  }
}

template<typename GameType>
void RegretMinimizer<GameType>::Train(int iterations) {
  std::array<float,GameType::PlayerNum> value;
  for (int i = 0; i < iterations; ++i) {
    for (int p = 0; p < GameType::PlayerNum; ++p) {
      value[p] = ExternalSamplingCFR(Game, p, 1.0, 1.0);
    }
    Game.reInitialize();
  }
}
template<typename GameType>
auto RegretMinimizer<GameType>::ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) -> float {
  ++nodesTouched;

  std::string type = game.getType();

  if ("terminal" == type) {
    return game.getUtility(updatePlayer);
  }
  if ("chance" == type) {
    /// get actions and their size
    std::vector<typename GameType::Action> const actions = game.getActions();

    /// sample one chance outcome at each chance node
    GameType copiedGame(game);
    copiedGame.transition(GameType::Action::None);
    float nodeValue;
    nodeValue = ChanceCFR(copiedGame, updatePlayer, probCounterFactual, probUpdatePlayer);
    return nodeValue;
  }
  if ("action" == type) { //Decision Node
    /// get actions and their size
    std::vector<typename GameType::Action> const actions = game.getActions();
    const auto actionNum = static_cast<int>(actions.size());
    float nodeValue = 0.f;

    Node *node = nodeMap[game.getInfoSet(game.getCurrentPlayer())];
    if (node == nullptr) {
      node = new Node(actionNum);
      nodeMap[game.getInfoSet(game.getCurrentPlayer())] = node;
    }

    const std::vector<float> currentStrategy = node->getStrategy();

    /// get counterfactual value and node value by recursively getting utilities and probability we reach them
    std::vector<float> counterfactualValue(actionNum);
    for (int i = 0; i < actionNum; ++i) {
      GameType gamePlusOneAction(game);
      gamePlusOneAction.transition(actions[i]);
      if (updatePlayer == game.getCurrentPlayer()) {
        counterfactualValue[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probCounterFactual, probUpdatePlayer * currentStrategy[i]);
      } else {
        counterfactualValue[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probCounterFactual * currentStrategy[i], probUpdatePlayer);
      }
      nodeValue += currentStrategy[i] * counterfactualValue[i];
    }

    /// do regret calculation and matching based on the returned nodeValue only for update player
    if (updatePlayer == game.getCurrentPlayer()) {
      for (int i = 0; i < actions.size(); ++i) {
        const float actionRegret = counterfactualValue[i] - nodeValue;
        node->updateRegretSum(i, actionRegret, probCounterFactual);
      }
      /// update average getStrategy across all training iterations
      node->updateStrategySum(currentStrategy, probUpdatePlayer);
      node->calcUpdatedStrategy();
    }
    return nodeValue;
  }
  throw GameStageViolation("did not match a game type in ChanceSamplingCFR");
}

template<typename GameType>
auto RegretMinimizer<GameType>::ExternalSamplingCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) -> float {
  ++nodesTouched;

  std::string type = game.getType();

  if ("terminal" == type) {
    return game.getUtility(updatePlayer);
  }

  //actions available at this game state / node
  const auto actions = game.getActions();
  const auto actionNum = static_cast<uint8_t >(actions.size());

  if ("chance" == type) {
    //sample one chance outcome at each chance node
    GameType copiedGame(game);
    copiedGame.transition(GameType::Action::None);
    float nodeValue;
    nodeValue = ExternalSamplingCFR(copiedGame, updatePlayer, probCounterFactual , probUpdatePlayer);
    return nodeValue;
  }

  if ("action" == type) { //Decision Node
    float nodeValue = 0.f;

    Node *node = nodeMap[game.getInfoSet(game.getCurrentPlayer())];
    if (node == nullptr) {
      node = new Node(actionNum);
      nodeMap[game.getInfoSet(game.getCurrentPlayer())] = node;
    }

    const std::vector<float> currentStrategy = node->getStrategy();
    std::vector<float> counterfactualValue(actionNum);
    if (updatePlayer == game.getCurrentPlayer()) {
      for (int i = 0; i < actionNum; ++i) {
        GameType gamePlusOneAction(game); // copy current gamestate
        gamePlusOneAction.transition(actions[i]); // go one level deeper with action i
        counterfactualValue[i] = ExternalSamplingCFR(gamePlusOneAction, updatePlayer, probCounterFactual, probUpdatePlayer * currentStrategy[i]);
        nodeValue += currentStrategy[i] * counterfactualValue[i];
      }

      for (int i = 0; i < actionNum; ++i) {
        const float regret = counterfactualValue[i] - nodeValue;
        node->updateRegretSum(i, regret, probCounterFactual);
      }
      node->updateStrategySum(currentStrategy, probUpdatePlayer);

      node->calcUpdatedStrategy();


    } else { //sample single player action for non update player
      GameType gamePlusOneAction(game);
      std::discrete_distribution<int> actionSpread(currentStrategy.begin(),currentStrategy.end());
      auto sampledAction = actionSpread(rng);
      gamePlusOneAction.transition(actions[sampledAction]);
      nodeValue = ExternalSamplingCFR(gamePlusOneAction, updatePlayer, probCounterFactual, probUpdatePlayer);
    }
    return nodeValue;
  }
  throw GameStageViolation("did not match a game type in ExternalSamplingCFR");
}
template <typename GameType>
auto RegretMinimizer<GameType>::getNodeInformation(const std::string& index) noexcept -> std::vector<std::vector<float>>{
  std::vector<std::vector<float>> res;
  res.push_back(nodeMap[index]->getRegretSum());
  res.push_back(nodeMap[index]->getStrategy());
  nodeMap[index]->calcAverageStrategy();
  res.push_back(nodeMap[index]->getAverageStrategy());
  return res;
}
}
#endif //INC_2PLAYERCFR_REGRETMINIMIZER_HPP
