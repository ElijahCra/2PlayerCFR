//
// Created by Elijah Crain on 8/27/23.
//

#include "RegretMinimizer.hpp"
#include <iostream>



RegretMinimizer::RegretMinimizer(const uint32_t seed) : mRNG(seed),
                                                        mNodeCount(0),
                                                        util()
{
    mGame = new Game(mRNG);
}

RegretMinimizer::~RegretMinimizer() {
    for (auto &itr : mNodeMap) {
        delete itr.second;
    }
    delete mGame;
}

void RegretMinimizer::Train(int iterations) {
    double utilities[PlayerNum];
    int rootAN =getRootChanceActionNum();
    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < PlayerNum; ++p) {
            utilities[p] = ChanceCFR(*mGame, p, (1.0/rootAN), (1.0/rootAN));

        }
        for (auto & itr : mNodeMap) {
            itr.second->updateStrategy();
        }
        if (i%100 == 0 and i !=0) {
            //std::cout << utilities[0] << "\n";

            printf("fold: %f, raise: %f, call: %f \n",mNodeMap["4945"]->getStrategy()[0],mNodeMap["4945"]->getStrategy()[1],mNodeMap["4945"]->getStrategy()[2]);
        }
        mGame->reInitialize();

    }
}


double RegretMinimizer::ChanceCFR(const Game& game, int updatePlayer, double probP0, double probP1) {
    ++mNodeCount;

    std::string type = game.getType();

    if ("terminal" == type){
        return game.getUtility(updatePlayer);
    }

     std::vector<Action> const actions  = game.getActions();

    //int const actionNum = static_cast<int>(actions.size());

    if ("chance" == type) {
        double weightedUtil;
        //sample one chance outcomes
        Game copiedGame(game);
        copiedGame.transition(Action::None);
        weightedUtil = ChanceCFR(copiedGame, updatePlayer, probP0, probP1);

        return weightedUtil;
    }
    else if ("action" == type) { //Decision Node


        double weightedUtil = 0;

        Node *node = mNodeMap[game.getInfoSet(game.mCurrentPlayer)];
        if (node == nullptr) {
            node = new Node(static_cast<int>(actions.size()));
            mNodeMap[game.getInfoSet(game.mCurrentPlayer)] = node;
        }

        const double *currentStrategy = node->getStrategy();

        double oneActionWeightedUtil[actions.size()];
        for (int i=0; i<actions.size();++i) {
            Game gamePlusOneAction(game);
            gamePlusOneAction.transition(actions[i]);
            if (0 == game.mCurrentPlayer) {
                oneActionWeightedUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probP0 * currentStrategy[i], probP1);
            }else {
                oneActionWeightedUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probP0, probP1 * currentStrategy[i]);
            }
             weightedUtil += currentStrategy[i] * oneActionWeightedUtil[i];
        }

        /// do regret calculation and matching based on the returned weightedUtil

        if (updatePlayer == game.mCurrentPlayer){
            if (updatePlayer == 0){
                double counterfacProb = probP1;
            } else {
                double counterfacProb = probP0;
            }
            for (int i=0; i<actions.size();++i) {
                const double regret = oneActionWeightedUtil[i] - weightedUtil;
                const double regretSum = node->regretSum(i) + probP0 * regret;
                node->regretSum(i, regretSum);
            }
            // update average getStrategy across all training iterations
            node->strategySum(node->getStrategy(), probP1);
        }
        return weightedUtil;
    }
    else {throw std::logic_error("not terminal action or chance type");}
}

