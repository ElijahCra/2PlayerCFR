//
// Created by Elijah Crain on 8/27/23.
//

#include "RegretMinimizer.hpp"



RegretMinimizer::RegretMinimizer(const uint32_t seed) : mRNG(seed),
                                                        mNodeCount(0),
                                                        util(){
    mGame = new Game(mRNG);
}

RegretMinimizer::~RegretMinimizer() {
    for (auto &itr : mNodeMap) {
        delete itr.second;
    }
    delete mGame;
}

void RegretMinimizer::Train(int iterations) {
    double utils[PlayerNum];

    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < PlayerNum; ++p) {
            utils[p] = ChanceCFR(*mGame, p, 1.0*1.0/getRootChanceActionNum(), 1.0*1.0/getRootChanceActionNum());
        }
    }
}


double RegretMinimizer::ChanceCFR(const Game& game, int updatePlayer, double probP0, double probP1) {
    ++mNodeCount;

    if ("terminal" == game.getType()){
        return game.getUtility(updatePlayer);
    }

     std::vector<Action> const actions  = game.getActions();

    //int const actionNum = static_cast<int>(actions.size());

    if ("chance" == game.getType()) {
        double weightedUtil;
        //sample one chance outcomes
        Game copiedGame(game);
        copiedGame.transition(Action::None);
        weightedUtil = ChanceCFR(copiedGame, updatePlayer, probP0, probP1);

        return weightedUtil;
    }
    else if ("action" == game.getType()) { //Decision Node
        double weightedUtil = 0;

        Node *node = mNodeMap[game.getInfoSet(game.mCurrentPlayer)];
        if (node == nullptr) {
            node = new Node(static_cast<int>(actions.size()));
            mNodeMap[game.getInfoSet(game.mCurrentPlayer)] = node;
        }
        double oneActionUtil[actions.size()];
        for (int i=0; const Action& action : actions) {
            Game gamePlusOneAction(game);
            gamePlusOneAction.transition(action);
            if (0 == game.mCurrentPlayer) {
                oneActionUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probP0 * node->getStrategy()[i], probP1);
            }else {
                oneActionUtil[i] = ChanceCFR(gamePlusOneAction, updatePlayer, probP0, probP1 * node->getStrategy()[i]);
            }
             weightedUtil += node->getStrategy()[i] * oneActionUtil[i];
            ++i;
        }

        /// do regret calculation and matching based on the returned weightedUtil

        if (updatePlayer == game.mCurrentPlayer){
            for (int i=0; i<actions.size();++i) {
                const double regret = oneActionUtil[i] - weightedUtil;
                const double regretSum = node->regretSum(i) + probP0 * regret;
                node->regretSum(i, regretSum);
                ++i;
            }
            // update average getStrategy across all training iterations
            node->strategySum(node->getStrategy(), probP1);
        }
        return weightedUtil;
    }
}

