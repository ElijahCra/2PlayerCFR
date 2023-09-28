//
// Created by Elijah Crain on 8/27/23.
//

#include "RegretMinimizer.hpp"
#include <iostream>

namespace CFR {

    template<typename GameType>
    RegretMinimizer<GameType>::RegretMinimizer(const uint32_t seed) : mRNG(seed),
                                                                      mNodeCount(0),
                                                                      util() {
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
        double utilities[GameType::PlayerNum];
        int rootAN = GameType::getRootChanceActionNum();
        for (int i = 0; i < iterations; ++i) {
            for (int p = 0; p < GameType::PlayerNum; ++p) {
                utilities[p] = ChanceCFR(*mGame, p, 1.0, 1.0);
            }
            mGame->averageUtilitySum += utilities[1];
            mGame->averageUtility = mGame->averageUtilitySum / (double(i));

            for (auto &itr: mNodeMap) {
                itr.second->updateStrategy();
            }
            if (i % 1000 == 0 and i != 0) {
                //std::cout << utilities[0] << "\n";
                std::cout << mGame->averageUtility << "\n";
                printf("fold: %f, raise: %f, call: %f \n", mNodeMap["2117"]->getStrategy()[0],
                       mNodeMap["2117"]->getStrategy()[1], mNodeMap["2117"]->getStrategy()[2]);
            }
            mGame->reInitialize();
        }
    }

    template<typename GameType>
    double RegretMinimizer<GameType>::ChanceCFR(const GameType &game, int updatePlayer, double probCounterFactual,
                                                double probUpdatePlayer) {
        ++mNodeCount;

        std::string type = game.getType();

        if ("terminal" == type) {
            return game.getUtility(updatePlayer);
        }

        auto const actions = game.getActions();

        int const actionNum = static_cast<int>(actions.size());

        if ("chance" == type) {
            double weightedUtil;
            //sample one chance outcomes
            GameType copiedGame(game);
            copiedGame.transition(GameType::Action::None);
            weightedUtil = ChanceCFR(copiedGame, updatePlayer, probCounterFactual * GameType::getRootChanceActionNum(),
                                     probUpdatePlayer);

            return weightedUtil;
        } else if ("action" == type) { //Decision Node

            double weightedUtil = 0;

            Node *node = mNodeMap[game.getInfoSet(game.currentPlayer)];
            if (node == nullptr) {
                node = new Node(static_cast<int>(actionNum));
                mNodeMap[game.getInfoSet(game.currentPlayer)] = node;
            }

            const double *currentStrategy = node->getStrategy();

            double oneActionWeightedUtil[actionNum];
            for (int i = 0; i < actionNum; ++i) {
                GameType gamePlusOneAction(game);
                gamePlusOneAction.transition(actions[i]);
                if (updatePlayer == game.currentPlayer) {
                    oneActionWeightedUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probCounterFactual,
                                                         probUpdatePlayer * currentStrategy[i]);
                } else {
                    oneActionWeightedUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer,
                                                         probCounterFactual * currentStrategy[i], probUpdatePlayer);
                }
                weightedUtil += currentStrategy[i] * oneActionWeightedUtil[i];
            }

            /// do regret calculation and matching based on the returned weightedUtil

            if (updatePlayer == game.currentPlayer) {
                for (int i = 0; i < actions.size(); ++i) {
                    const double regret = oneActionWeightedUtil[i] - weightedUtil;
                    const double regretSum = node->regretSum(i) + probCounterFactual * regret;
                    node->regretSum(i, regretSum);
                }
                // update average getStrategy across all training iterations
                node->strategySum(node->getStrategy(), probUpdatePlayer);
            }
            return weightedUtil;
        } else { throw std::logic_error("not terminal action or chance type"); }
    }
}
