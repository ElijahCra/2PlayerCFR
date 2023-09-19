//
// Created by Elijah Crain on 8/27/23.
//

#include "RegretMin.hpp"



RegretMin::RegretMin(const uint32_t seed) : mRNG(seed), mNodeCount(0) {
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

    if ("terminal" == game.getCurrentState()->type()){
        return game.getUtility(playerNum);
    }

     std::vector<Action> const actions  = game.getActions();

    int const actionNum = static_cast<int>(actions.size());

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

        Node *node = mNodeMap[game.getInfoSet(game.mCurrentPlayer)];
        if (node == nullptr) {
            node = new Node(static_cast<int>(actions.size()));
            mNodeMap[game.getInfoSet(game.mCurrentPlayer)] = node;
        }

        for (int i=0; const Action& action : actions) {
            Game copiedGame(game);
            copiedGame.transition(action);
            weightedUtil = node->getStrategy()[i] * ChanceCFR(copiedGame, playerNum, probP0, probP1, probChance);
            ++i;
        }

        /// do regret calculation and matching based on the returned weightedUtil


        double regret[3]{0};
        for (int i=0; auto action : actions) {
            if (0 == playerNum) {
                regret[i] = probP1 * probChance * (weightedUtil / node->getStrategy()[i] - weightedUtil);
            }
            else {
                regret[i] = probP0 * probChance * (weightedUtil / node->getStrategy()[i] - weightedUtil);
            }
            ++i;
        }
        return weightedUtil;
    }
}

