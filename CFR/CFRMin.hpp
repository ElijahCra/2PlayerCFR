//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_CFRMIN_HPP
#define INC_2PLAYERCFR_CFRMIN_HPP

#include <random>


class CFRMin {
public:
    CFRMin();

    void RegretMinimizer();

    void CFR();
private:
    std::mt19937 mRNG;

};


#endif //INC_2PLAYERCFR_CFRMIN_HPP
