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
        /// @param probChance probability of reaching current history given only the chance contributions
        float ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual, float probUpdatePlayer);

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
                utilities[p] = ChanceCFR(*mGame, p, 1.0, 1.0);
            }
            mGame->averageUtilitySum += utilities[1];
            mGame->averageUtility = mGame->averageUtilitySum / ((float)i);

            for (auto &itr: mNodeMap) {
                itr.second->updateStrategy();
            }
            if (i % 100 == 0 and i > 1000) {
                //std::cout << utilities[0] << "\n";
                std::cout << mGame->averageUtility << "\n";

                printf("raise: %.9g, call: %.9g, fold: %.9g, iteration: %d \n", mNodeMap["0502"]->getRegretSum(0),
                       mNodeMap["0502"]->getRegretSum(1), mNodeMap["0502"]->getRegretSum(2), i);

                printf("raise: %f, call: %f, fold: %f \n", mNodeMap["0502"]->getAverageStrategy()[0],
                       mNodeMap["0502"]->getAverageStrategy()[1], mNodeMap["0502"]->getAverageStrategy()[2]);
            }
            mGame->reInitialize();
        }
    }

    template<typename GameType>
    float RegretMinimizer<GameType>::ChanceCFR(const GameType &game, int updatePlayer, float probCounterFactual,
                                               float probUpdatePlayer) {
        ++mNodeCount;

        std::string type = game.getType();

        if ("terminal" == type) {
            return game.getUtility(updatePlayer);
        }

        auto const actions = game.getActions();

        int const actionNum = static_cast<int>(actions.size());

        if ("chance" == type) {
            //sample one chance outcome at each chance node
            GameType copiedGame(game);
            copiedGame.transition(GameType::Action::None);
            float weightedUtil;
            weightedUtil = ChanceCFR(copiedGame, updatePlayer, probCounterFactual * (1.0/game.getChanceActionNum()), probUpdatePlayer);
            return weightedUtil;
        }

        else if ("action" == type) { //Decision Node

            float weightedUtil = 0.f;

            Node *node = mNodeMap[game.getInfoSet(game.currentPlayer)];
            if (node == nullptr) {
                node = new Node(static_cast<int>(actionNum));
                mNodeMap[game.getInfoSet(game.currentPlayer)] = node;
            }

            const float *currentStrategy = node->getStrategy();

            float oneActionWeightedUtil[actionNum];
            for (int i = 0; i < actionNum; ++i) {
                GameType gamePlusOneAction(game);
                gamePlusOneAction.transition(actions[i]);
                if (updatePlayer == game.currentPlayer) {
                    oneActionWeightedUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probCounterFactual, probUpdatePlayer * currentStrategy[i]);
                } else {
                    oneActionWeightedUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probCounterFactual * currentStrategy[i], probUpdatePlayer);
                }
                weightedUtil += currentStrategy[i] * oneActionWeightedUtil[i];
            }

            /// do regret calculation and matching based on the returned weightedUtil

            if (updatePlayer == game.currentPlayer) {
                for (int i = 0; i < actions.size(); ++i) {
                    const float regret = oneActionWeightedUtil[i] - weightedUtil;
                    const float regretSum = node->getRegretSum(i) + probCounterFactual * regret;
                    node->updateRegretSum(i, regretSum);
                }
                // update average getStrategy across all training iterations
                node->updateStrategySum(currentStrategy, probUpdatePlayer);
            }
            return weightedUtil;
        } else { throw std::logic_error("not terminal action or chance type"); }
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
