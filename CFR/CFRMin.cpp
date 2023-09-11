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


double CFRMin::VanillaCFR(const Game& game, int playerNum, double probP0, double probP1, double probChance) {
    ++mNodeCount;

    const std::vector<Action> actions = game.getActions();

    if (GameStates::Terminal == game.getCurrentState()->type()) {
        return payoff();
    }
    else if (GameStates::PreFlopChance == game.getCurrentState()->type()){
        double nodeVal;
        //sample all chance outcomes
       Game copiedGame(game);
       for (auto action : actions){
           copiedGame.transition(action);
           nodeVal =  VanillaCFR(copiedGame, playerNum, probP0, probP1, 1.0/getRootChanceActionNum());
       }
       return nodeVal;

    }
    else if (GameStates::PreFlopActionNoBet == game.getCurrentState()->type()){ //Preflop Action
        double value = 0;
        double cfValue[3] {0};

        for (int i =0; i<actions.size(); ++i) {
            Game copiedGame(game);
            copiedGame.transition(actions[i]);
            cfValue[i] = VanillaCFR(copiedGame, playerNum, probP0, probP1, probChance);
        }

    }

}