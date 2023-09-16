//
// Created by Elijah Crain on 8/27/23.
//

#include "CFRMin.hpp"
#include "../Game/Game.hpp"


CFRMin::CFRMin(const uint32_t seed) : mRNG(seed) {
    mGame = new Game(mRNG);
}

CFRMin::~CFRMin() {
    for (auto &itr : mNodeMap) {
        delete itr.second;
    }
    delete mGame;
}

void CFRMin::Train(int iterations) {
    double utils[PlayerNum];

    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < PlayerNum; ++p) {
            utils[p] = VanillaCFR(*mGame, p, 1.0, 1.0, 1.0/getRootChanceActionNum());
            for (auto & itr : mNodeMap) {
                itr.second->updateStrategy();
            }
        }
    }
}


double CFRMin::ChanceCFR(const Game& game, int playerNum, double probP0, double probP1, double probChance) {
    ++mNodeCount;

    Action const * const actions  = game.getActions();

    int const actionNum = sizeof(*actions) / sizeof(Action);

    if (GameStates::PreFlopChance == game.getCurrentState()->type()) {
        double weightedUtil;
        //sample one chance outcomes
        Game copiedGame(game);
        copiedGame.transition(Action::None);
        weightedUtil = ChanceCFR(copiedGame, playerNum, probP0, probP1, 1.0 / getRootChanceActionNum());

        return weightedUtil;
    }

    else { //Decision Node
        double weightedUtil;

        for (int i =0; i<actionNum; ++i) {
            Game copiedGame(game);
            copiedGame.transition(actions[i]);
            weightedUtil = game.nodeProbability * ChanceCFR(copiedGame, playerNum, probP0, probP1, probChance);
        }

        /// do regret calculation and matching based on the returned weightedUtil
        double regret[3];

        for (int i=0; i<actionNum; ++i) {
            reget[i] = probP0 or probP1 * (weightedUtil/game.strategy[i] - weightedUtil)
        }



        return weightedUtil;

    }

}