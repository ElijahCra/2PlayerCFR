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


double CFRMin::VanillaCFR(const Game& game, int playerNum, double probActing, double probNotActing, double probChance) {
    ++mNodeCount;
    double nodeUtil = 0.0;

    const std::vector<Action> actions = game.getActions();

    if (GameStates::Terminal == game.getCurrentState()->type()) {
        return payoff();
    }
    else if (GameStates::PreFlopChance == game.getCurrentState()->type()){
        //sample all chance outcomes
       Game copiedGame(game);
       for (auto action : actions){
           copiedGame.transition(action);
           nodeUtil =  VanillaCFR(copiedGame, playerNum, 1.0, 1.0, 1.0/getRootChanceActionNum());
       }
       return nodeUtil;

    }
    else { //Preflop Action


        double cfValue[actions.size()];
    }

}