//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_CFRMIN_HPP
#define INC_2PLAYERCFR_CFRMIN_HPP

#include <random>
#include "Game.hpp"
#include "GameState.hpp"


class CFRMin {
public:
    CFRMin(const uint32_t seed = std::random_device()());

    void RegretMinimizer();

    void CFR();
private:
    std::mt19937 mRNG;

};


#endif //INC_2PLAYERCFR_CFRMIN_HPP
