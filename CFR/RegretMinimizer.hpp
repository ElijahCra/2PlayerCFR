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

namespace CFR {

template<typename GameType>
class RegretMinimizer {
public:
    /// @brief constructor takes a seed or one is generated
    explicit RegretMinimizer(uint32_t seed = std::random_device()());

    ~RegretMinimizer();

    /// @brief calls cfr algorithm for full game tree (or sampled based on version) traversal the specified number of times
    void Train(int iterations);

    /// @brief calls cfr algorithm the specified number of times
    void multiTrain(int iterations);

    /// @brief recursively traverse game tree (depth-first) sampling only one chance outcome at each chance node and all actions
    /// @param updatePlayer player whose getStrategy is updated and utilities are retrieved in terms of
    /// @param probCounterFactual probability of reaching the next node given all players actions and chance except for the updatePlayer's actions
    /// @param probUpdatePlayer probability of reaching next node given only the updatePlayer's actions
    /// @param game the next node in the dfs
    /// probCounterfactual does not include chance probabilities bc we divivide by the probability the chance outcomes were sampled making them 1 for the calculation
    float ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer);

    /// @brief same as ChanceCFR except at each action node sample one action for non update player
    float ExternalSamplingCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer);

private:
    std::mt19937 rng;

    Utility util;

    std::unordered_map<std::string, Node *> nodeMap;

    GameType *Game;

    uint64_t nodesTouched;


    void preGenTree(GameType &game);
    void preGenTreeMultiThreaded();
};


///Implementation of templates above
template<typename GameType>
RegretMinimizer<GameType>::RegretMinimizer(const uint32_t seed) : rng(seed), util(), nodesTouched(0) {
        Game = new GameType(rng);
}

template<typename GameType>
RegretMinimizer<GameType>::~RegretMinimizer() {
    for (auto &itr: nodeMap) {
        delete itr.second;
    }
    delete Game;
}

template<typename GameType>
void RegretMinimizer<GameType>::Train(int iterations) {
    float value[GameType::PlayerNum];
    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            value[p] = ExternalSamplingCFR(*Game, p, 1.0, 1.0);
        }
        Game->averageUtilitySum += value[1];
        Game->averageUtility = Game->averageUtilitySum / ((float)i);

        if (i % 1000 == 0 and i > 5000) {
            //std::cout << value[0] << "\n";
            //std::cout << Game->averageUtility << "\n";
            
            auto regretSum = nodeMap["90"]->getRegretSum();
            auto currentStrat = nodeMap["90"]->getStrategy();
            nodeMap["90"]->calcAverageStrategy();
            auto averageStrat = nodeMap["90"]->getAverageStrategy();
            printf("  \nValues for a pair of Aces on the first round first action \nAverage Utility: %.4g \nRegret Sum \n raise: %.4g, call: %.4g, fold: %.4g, iteration: %d \nStrategy on this iteration \n raise: %.3g%%, call: %.3g%%, fold: %.3g%% \nAverage Strategy \n raise: %.3g%%, call: %.3g%%, fold: %.3g%% \n",
            Game->averageUtility, regretSum[0],regretSum[1], regretSum[2], i,currentStrat[0]*100, currentStrat[1]*100, currentStrat[2]*100, averageStrat[0]*100, averageStrat[1]*100, averageStrat[2]*100);
        }
        Game->reInitialize();
    }
}

template<typename GameType>
void RegretMinimizer<GameType>::multiTrain(int iterations) {
    float utilities[GameType::PlayerNum];
    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            utilities[p] = ExternalSamplingCFR(*Game, p, 1.0, 1.0);
        }
        Game->averageUtilitySum += utilities[1];
        Game->averageUtility = Game->averageUtilitySum / ((float)i);

        if (i % 1000 == 0 and i > 1000) {
            //std::cout << utilities[0] << "\n";
            std::cout << Game->averageUtility << "\n";

            auto regretSum = nodeMap["1211"]->getRegretSum();
            printf("raise: %.6g, call: %.6g, fold: %.6g, iteration: %d \n", regretSum[0],
                   regretSum[1], regretSum[2], i);

            nodeMap["1211"]->calcAverageStrategy();
            auto averageStrat = nodeMap["1211"]->getAverageStrategy();
            printf("raise: %.6g, call: %.6g, fold: %.6g \n", averageStrat[0],
                   averageStrat[1], averageStrat[2]);
        }
        Game->reInitialize();
    }
}

template<typename GameType>
float RegretMinimizer<GameType>::ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) {
    ++nodesTouched;

    std::string type = game.getType();

    if ("terminal" == type) {
        return game.getUtility(updatePlayer);
    }
    else if ("chance" == type) {
        /// get actions and their size
        std::vector<typename GameType::Action> const actions = game.getActions();

        /// sample one chance outcome at each chance node
        GameType copiedGame(game);
        copiedGame.transition(GameType::Action::None);
        float nodeValue;
        nodeValue = ChanceCFR(copiedGame, updatePlayer, probCounterFactual, probUpdatePlayer);
        return nodeValue;
    }
    else if ("action" == type) { //Decision Node
        /// get actions and their size
        std::vector<typename GameType::Action> const actions = game.getActions();
        int const actionNum = static_cast<int>(actions.size());
        float nodeValue = 0.f;

        Node *node = nodeMap[game.getInfoSet(game.currentPlayer)];
        if (node == nullptr) {
            node = new Node(actionNum);
            nodeMap[game.getInfoSet(game.currentPlayer)] = node;
        }

        const std::vector<float> currentStrategy = node->getStrategy();

        /// get counterfactual value and node value by recursively getting utilities and probability we reach them
        std::vector<float> counterfactualValue(actionNum);
        for (int i = 0; i < actionNum; ++i) {
            GameType gamePlusOneAction(game);
            gamePlusOneAction.transition(actions[i]);
            if (updatePlayer == game.currentPlayer) {
                counterfactualValue[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probCounterFactual, probUpdatePlayer * currentStrategy[i]);
            } else {
                counterfactualValue[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probCounterFactual * currentStrategy[i], probUpdatePlayer);
            }
            nodeValue += currentStrategy[i] * counterfactualValue[i];
        }

        /// do regret calculation and matching based on the returned nodeValue only for update player
        if (updatePlayer == game.currentPlayer) {
            for (int i = 0; i < actions.size(); ++i) {
                const float actionRegret = counterfactualValue[i] - nodeValue;
                node->updateRegretSum(i, actionRegret, probCounterFactual);
            }

            /// update average getStrategy across all training iterations
            node->updateStrategySum(currentStrategy, probUpdatePlayer);

            node->calcUpdatedStrategy();
        }

        return nodeValue;
    } else { throw std::logic_error("not terminal action or chance type"); }
}

template<typename GameType>
float RegretMinimizer<GameType>::ExternalSamplingCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer) {//, float probSample) {
    ++nodesTouched;

    std::string type = game.getType();

    if ("terminal" == type) {
        return game.getUtility(updatePlayer);
    }

    //actions available at this game state / node
    const auto actions = game.getActions();
    const int actionNum = static_cast<int>(actions.size());

    if ("chance" == type) {
        //sample one chance outcome at each chance node
        GameType copiedGame(game);
        copiedGame.transition(GameType::Action::None);
        float nodeValue;
        nodeValue = ExternalSamplingCFR(copiedGame, updatePlayer, probCounterFactual , probUpdatePlayer);
        return nodeValue;
    }

    else if ("action" == type) { //Decision Node
        float nodeValue = 0.f;

        Node *node = nodeMap[game.getInfoSet(game.currentPlayer)];
        if (node == nullptr) {
            node = new Node(actionNum);
            nodeMap[game.getInfoSet(game.currentPlayer)] = node;
        }

        const std::vector<float> currentStrategy = node->getStrategy();
        std::vector<float> counterfactualValue(actionNum);
        if (updatePlayer == game.currentPlayer) {
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
    throw(std::logic_error("this shouldnt have been reached in external sampling"));
}


template<typename GameType>
void RegretMinimizer<GameType>::preGenTree(GameType &game) {
    for (int i=0; i<game.getChanceActionNum(); ++i) {
        std::string type = game.getType();

    }
}

template<typename GameType>
void RegretMinimizer<GameType>::preGenTreeMultiThreaded() {
    //todo
}

}
#endif //INC_2PLAYERCFR_REGRETMINIMIZER_HPP
