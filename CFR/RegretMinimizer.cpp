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
        double weightedUtil;

        Node *node = mNodeMap[game.getInfoSet(game.mCurrentPlayer)];
        if (node == nullptr) {
            node = new Node(static_cast<int>(actions.size()));
            mNodeMap[game.getInfoSet(game.mCurrentPlayer)] = node;
        }

        for (int i=0; const Action& action : actions) {
            Game copiedGame(game);
            copiedGame.transition(action);
            if (0 == game.mCurrentPlayer) {
                weightedUtil = node->getStrategy()[i] * ChanceCFR(copiedGame, updatePlayer, probP0*node->getStrategy()[i], probP1);
            }else {
                weightedUtil = node->getStrategy()[i] * ChanceCFR(copiedGame, updatePlayer, probP0, probP1*node->getStrategy()[i]);
            }
            ++i;
        }

        /// do regret calculation and matching based on the returned weightedUtil

        if (updatePlayer == game.mCurrentPlayer){
            double regret[3]{0};
            for (int i=0; auto action : actions) {
                if (0 == updatePlayer) {
                    regret[i] = probP1 * (weightedUtil / node->getStrategy()[i] - weightedUtil);
                }
                else {
                    regret[i] = probP0 * (weightedUtil / node->getStrategy()[i] - weightedUtil);
                }
                ++i;
            }
        }

        return weightedUtil;
    }
}

