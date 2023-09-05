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
            utils[p] = VanillaCFR(*mGame, p, 1.0, 1.0);
            for (auto & itr : mNodeMap) {
                itr.second->updateStrategy();
            }
        }
    }
}


double CFRMin::VanillaCFR(const Game& game, int playerNum, double reachCF, double reachSoloCF) {
    ++mNodeCount;

}