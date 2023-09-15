//
// Created by elijah on 8/29/2023.
//

#ifndef INC_2PLAYERCFR_NODE_HPP
#define INC_2PLAYERCFR_NODE_HPP

#include <array>
#include "../Game/Game.hpp"


class Node {
public:
    Node(const Action actionNum);
    ~Node();

    const double* strategy();

    const double *averageStrategy();

    void strategySum(const double *strategy, double realizationWeight);

    void updateStrategy();

    double regretSum(int action) const;

    void regretSum(int action, double value);

    uint8_t actionNum() const;
private:
    void calcAverageStrategy();

    double *mStrats;
    double *mStrateSums;
    double *mInstRegretSums;

    double *mAverageStra;

    int *mNormalizingSum;
    const int mActionNum;
    bool mAlreadyCalculated;
    bool mNeedToUpdateStrategy;

};


#endif //INC_2PLAYERCFR_NODE_HPP
