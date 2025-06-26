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
#include <memory>
#include "Node.hpp"
#include "../Storage/NodeStorage.hpp"
#include "../Storage/MapNodeStorage.hpp"
#include "../Game/Utility/Utility.hpp"
#include "CustomExceptions.h"

namespace CFR {

template<typename GameType, typename StorageType = MapNodeStorage>
class RegretMinimizer {
 public:
  /// @brief constructor takes a seed or one is generated
  explicit RegretMinimizer(uint32_t seed = std::random_device()());
  
  /// @brief constructor with custom storage
  RegretMinimizer(uint32_t seed, std::shared_ptr<StorageType> storage);
  
  RegretMinimizer(const RegretMinimizer& other) = delete;
  auto operator=(const RegretMinimizer& other) -> RegretMinimizer& = delete;
  RegretMinimizer(RegretMinimizer&& other) = default;
  auto operator=(RegretMinimizer&& other) -> RegretMinimizer& = default;
  ~RegretMinimizer() = default;

  /// @brief calls cfr algorithm for full game tree (or sampled based on version) traversal the specified number of times
  void Train(uint32_t iterations);

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

  std::shared_ptr<StorageType> m_storage;

  GameType Game;

  uint64_t nodesTouched{};

};


///Implementation of templates above
template<typename GameType, typename StorageType>
RegretMinimizer<GameType, StorageType>::RegretMinimizer(const uint32_t seed) 
    : rng(seed), m_storage(std::make_unique<StorageType>()), Game(rng) {}

template<typename GameType, typename StorageType>
RegretMinimizer<GameType, StorageType>::RegretMinimizer(uint32_t seed, std::shared_ptr<StorageType> storage)
    : rng(seed), m_storage(std::move(storage)), Game(rng) {}

template<typename GameType, typename StorageType>
void RegretMinimizer<GameType, StorageType>::Train(uint32_t iterations) {
  std::array<float,GameType::PlayerNum> value;
  for (uint32_t i = 0; i < iterations; ++i) {
    for (uint32_t p = 0; p < GameType::PlayerNum; ++p) {
      value[p] = ExternalSamplingCFR(Game, p, 1.0, 1.0);
    }
    Game.reInitialize();
  }
}
template<typename GameType, typename StorageType>
auto RegretMinimizer<GameType, StorageType>::ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) -> float {
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

    std::string infoSet = game.getInfoSet(game.getCurrentPlayer());
    auto node = m_storage->getNode(infoSet);
    if (node == nullptr) {
      auto newNode = std::make_shared<Node>(actionNum);
      m_storage->putNode(infoSet, newNode);
      // Re-fetch to handle potential race condition where another thread inserted the same node
      node = m_storage->getNode(infoSet);
      if (node == nullptr) {
        // Fallback: use our newly created node if storage still returns null
        node = newNode;
      }
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

template<typename GameType, typename StorageType>
auto RegretMinimizer<GameType, StorageType>::ExternalSamplingCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) -> float {
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

    std::string infoSet = game.getInfoSet(game.getCurrentPlayer());
    auto node = m_storage->getNode(infoSet);
    if (node == nullptr) {
      auto newNode = std::make_shared<Node>(actionNum);
      m_storage->putNode(infoSet, newNode);
      // Re-fetch to handle potential race condition where another thread inserted the same node
      node = m_storage->getNode(infoSet);
      if (node == nullptr) {
        // Fallback: use our newly created node if storage still returns null
        node = newNode;
      }
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
template <typename GameType, typename StorageType>
auto RegretMinimizer<GameType, StorageType>::getNodeInformation(const std::string& index) noexcept -> std::vector<std::vector<float>>{
  std::vector<std::vector<float>> res;
  auto node = m_storage->getNode(index);
  if (node != nullptr) {
    res.push_back(node->getRegretSum());
    res.push_back(node->getStrategy());
    node->calcAverageStrategy();
    res.push_back(node->getAverageStrategy());
  }
  return res;
}
}
#endif //INC_2PLAYERCFR_REGRETMINIMIZER_HPP
