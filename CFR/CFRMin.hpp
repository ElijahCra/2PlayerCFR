//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_CFRMIN_HPP
#define INC_2PLAYERCFR_CFRMIN_HPP

#include <random>
#include "../Game/Game.hpp"
//#include "../Game/GameState.hpp"
#include <unordered_map>
#include "Node.hpp"


class CFRMin {
public:
    explicit CFRMin(const uint32_t seed = std::random_device()());

    ~CFRMin();

    void Train(int iterations);


private:
    double VanillaCFR(const Game& game, int playerNum, double reachCF, double reachSoloCF);
    std::mt19937 mRNG;

    std::unordered_map<std::string, Node *> mNodeMap;
    uint64_t mNodeTouchedCnt;
    Game *mGame;

    int mNodeCount;

};


#endif //INC_2PLAYERCFR_CFRMIN_HPP
