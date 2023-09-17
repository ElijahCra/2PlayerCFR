//
// Created by Elijah Crain on 8/27/23.
//

#include "RegretMin.hpp"
#include "../Game/Game.hpp"


RegretMin::RegretMin(const uint32_t seed) : mRNG(seed) {
    mGame = new Game(mRNG);
}

RegretMin::~RegretMin() {
    for (auto &itr : mNodeMap) {
        delete itr.second;
    }
    delete mGame;
}

void RegretMin::Train(int iterations) {
    double utils[PlayerNum];

    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < PlayerNum; ++p) {
            utils[p] = ChanceCFR(*mGame, p, 1.0, 1.0, 1.0/getRootChanceActionNum());
        }
    }
}


double RegretMin::ChanceCFR(const Game& game, int playerNum, double probP0, double probP1, double probChance) {
    ++mNodeCount;

     Action const * const actions  = game.getActions();

    int const actionNum = sizeof(*actions) / sizeof(int);

    if ("chance" == game.getCurrentState()->type()) {
        double weightedUtil;
        //sample one chance outcomes
        Game copiedGame(game);
        copiedGame.transition(Action::None);
        weightedUtil = ChanceCFR(copiedGame, playerNum, probP0, probP1, 1.0 / getRootChanceActionNum());

        return weightedUtil;
    }
    else if ("action" == game.getCurrentState()->type()) { //Decision Node
        double weightedUtil;

        for (auto action : actions) {
            Game copiedGame(game);
            copiedGame.transition(actions[i]);
            weightedUtil = game.nodeProbability * ChanceCFR(copiedGame, playerNum, probP0, probP1, probChance);
        }

        /// do regret calculation and matching based on the returned weightedUtil
        Node *node = mNodeMap[game.mInfoSet[game.mCurrentPlayer]];
        if (node == nullptr) {
            node = new Node(actionNum);
            mNodeMap[game.mInfoSet[game.mCurrentPlayer]] = node;
        }
        double regret[3];

        for (int i=0; i<actionNum; ++i) {
            regret[i] = probP0 or probP1 * (weightedUtil/strategy[i] - weightedUtil)
        }



        return weightedUtil;

    }

}