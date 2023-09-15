//
// Created by elijah on 8/29/2023.
//

#ifndef INC_2PLAYERCFR_NODE_HPP
#define INC_2PLAYERCFR_NODE_HPP

#include <array>
#include "../Game/Game.hpp"


class Node {
public:
    Node(const int actionNum);
    ~Node();

    const double* getStrategy();

    int getActionNum() const;

    const double* getAverageStrategy();

private:
    void calcAverageStrategy();

    ///@brief number of actions available at this node
    const int actionNum;

    ///@brief array of cumulative counterfactual regret for each action over all iterations
    double* regretSum;

    ///@brief array of probability of taking each action, strategy of acting player at this node, values should add to 1.
    double* strategy;

    ///@brief array of cumulative probability of taking each action over all iterations
    double* strategySum;

    ///@brief array of calculated average strategy over all iterations;
    double* averageStrategy;

    ///@brief whether or not this nodes strategy should be updated
    bool updateStrategy;
};


#endif //INC_2PLAYERCFR_NODE_HPP
