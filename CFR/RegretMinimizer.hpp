//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_REGRETMINIMIZER_HPP
#define INC_2PLAYERCFR_REGRETMINIMIZER_HPP

#include <random>
#include "../Game/Preflop/Game.hpp"
#include "../Game/Texas/Game.hpp"
#include <unordered_map>
#include "Node.hpp"
#include "../Utility/Utility.hpp"

namespace CFR {

template<typename GameType>
class RegretMinimizer {
public:
    explicit RegretMinimizer(uint32_t seed = std::random_device()());

    ~RegretMinimizer();


    void Train(int iterations);

    /// @brief recursively traverse game tree (depth-first) sampling only one chance outcome at each chance node and all actions
    /// @param updatePlayer player whose getStrategy is updated and utilities are retrieved in terms of
    /// @param probCounterFactual reach contribution of all players and chance except for active player (who is the update player for this implementation)
    /// @param probUpdatePlayer reach contribution of only the active player (update player)
    float ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer, float probChance);

    /// @brief same as ChanceCFR except at each action node sample one action for non update player, but still sample all actions for update player
    float ExternalSamplingCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer, float probChance);

private:
    void preGenTree();
    void preGenTreeMultiThreaded();

    Utility util;

    std::mt19937 mRNG;

    std::unordered_map<std::string, Node *> mNodeMap;

    GameType *mGame;

    uint64_t mNodeCount;

};


///Implementation of templates above
template<typename GameType>
RegretMinimizer<GameType>::RegretMinimizer(const uint32_t seed) : mRNG(seed),
                                                                  util(),
                                                                  mNodeCount(0)
{
    mGame = new GameType(mRNG);
}

template<typename GameType>
RegretMinimizer<GameType>::~RegretMinimizer() {
    for (auto &itr: mNodeMap) {
        delete itr.second;
    }
    delete mGame;
}

template<typename GameType>
void RegretMinimizer<GameType>::Train(int iterations) {
    float utilities[GameType::PlayerNum];
    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            utilities[p] = ChanceCFR(*mGame, p, 1.0, 1.0, 1.0);
        }
        mGame->averageUtilitySum += utilities[1];
        mGame->averageUtility = mGame->averageUtilitySum / ((float)i);

        /*for (auto &itr: mNodeMap) {
            itr.second->calcUpdatedStrategy();
        }*/
        if (i % 100 == 0 and i > 1000) {
            //std::cout << utilities[0] << "\n";
            std::cout << mGame->averageUtility << "\n";

            auto regretSum = mNodeMap["1211"]->getRegretSum();
            printf("raise: %.6g, call: %.6g, fold: %.6g, iteration: %d \n", regretSum[0],
                   regretSum[1], regretSum[2], i);

            mNodeMap["1211"]->calcAverageStrategy();
            auto averageStrat = mNodeMap["1211"]->getAverageStrategy();
            printf("raise: %.6g, call: %.6g, fold: %.6g \n", averageStrat[0],
                   averageStrat[1], averageStrat[2]);
        }
        mGame->reInitialize();
    }
}

template<typename GameType>
float RegretMinimizer<GameType>::ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer, float probChance) {
    ++mNodeCount;

    std::string type = game.getType();

    if ("terminal" == type) {
        return game.getUtility(updatePlayer);
    }

    std::vector<typename GameType::Action> const actions = game.getActions();

    int const actionNum = static_cast<int>(actions.size());

    if ("chance" == type) {
        /// sample one chance outcome at each chance node
        GameType copiedGame(game);
        copiedGame.transition(GameType::Action::None);
        float weightedUtil;
        weightedUtil = ChanceCFR(copiedGame, updatePlayer, probCounterFactual / (float)game.getChanceActionNum(), probUpdatePlayer, probChance / (float)game.getChanceActionNum());
        return weightedUtil;
    }

    else if ("action" == type) { //Decision Node

        float nodeValue = 0.f;

        Node *node = mNodeMap[game.getInfoSet(game.currentPlayer)];
        if (node == nullptr) {
            node = new Node(actionNum);
            mNodeMap[game.getInfoSet(game.currentPlayer)] = node;
        }

        const float *currentStrategy = node->getStrategy();

        /// get counterfactual value and node value by recursively getting utilities and probability we reach them
        float counterfactualValue[actionNum];
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
float RegretMinimizer<GameType>::ExternalSamplingCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer, float probChance) {
    ++mNodeCount;

    std::string type = game.getType();

    if ("terminal" == type) {
        return game.getUtility(updatePlayer);
    }


    //actions available at this game state / node
    auto const actions = game.getActions();
    int const actionNum = static_cast<int>(actions.size());

    if ("chance" == type) {
        //sample one chance outcome at each chance node
        GameType copiedGame(game);
        copiedGame.transition(GameType::Action::None);
        float weightedUtil;
        weightedUtil = ExternalSamplingCFR(copiedGame, updatePlayer,
                                           probCounterFactual / (float) game.getChanceActionNum(), probUpdatePlayer,
                                           probChance / (float) game.getChanceActionNum());
        return weightedUtil;
    }

    else if ("action" == type) { //Decision Node
        float weightedUtil = 0.f;

        Node *node = mNodeMap[game.getInfoSet(game.currentPlayer)];
        if (node == nullptr) {
            node = new Node(actionNum);
            mNodeMap[game.getInfoSet(game.currentPlayer)] = node;
        }

        const float *currentStrategy = node->getStrategy();
        float oneActionWeightedUtil[actionNum];
        if (updatePlayer == game.currentPlayer) {
            for (int i = 0; i < actionNum; ++i) {
                GameType gamePlusOneAction(game); // copy current gamestate
                gamePlusOneAction.transition(actions[i]); // go one level deeper with action i
                oneActionWeightedUtil[i] = ExternalSamplingCFR(gamePlusOneAction, updatePlayer, probCounterFactual, probUpdatePlayer * currentStrategy[i]);
                weightedUtil += currentStrategy[i] * oneActionWeightedUtil[i];
            }

            for (int i = 0; i < actions.size(); ++i) {
                const float regret = oneActionWeightedUtil[i] - weightedUtil;
                node->updateRegretSum(i, regret, probCounterFactual);
            }
            node->updateStrategySum(currentStrategy, probUpdatePlayer);

            node->calcUpdatedStrategy();


        } else { //sample single player action for non update player
            GameType gamePlusOneAction(game);
            std::discrete_distribution<int> actionSpread(currentStrategy,currentStrategy+actionNum);
            auto sampledAction = actionSpread(mRNG);
            gamePlusOneAction.transition(actions[sampledAction]);
            weightedUtil = ExternalSamplingCFR(gamePlusOneAction, updatePlayer, probCounterFactual * currentStrategy[sampledAction], probUpdatePlayer);
        }
        return weightedUtil;
    }
}


template<typename GameType>
void RegretMinimizer<GameType>::preGenTree() {
    //todo
}

template<typename GameType>
void RegretMinimizer<GameType>::preGenTreeMultiThreaded() {
    //todo
}

}
#endif //INC_2PLAYERCFR_REGRETMINIMIZER_HPP
