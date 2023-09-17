//
// Created by elijah on 8/29/2023.
//

#include "Node.hpp"

Node::Node(const int actionNum) : actionNum(actionNum){
    regretSum = new double[actionNum];
    strategy = new double[actionNum];
    strategySum = new double[actionNum];
    averageStrategy = new double[actionNum];
    for (int a = 0; a < actionNum; ++a) {
        regretSum[a] = 0.0;
        strategy[a] = 1.0 / (double) actionNum;
        strategySum[a] = 0.0;
        averageStrategy[a] = 0.0;
    }
}

Node::~Node() {
    delete[] regretSum;
    delete[] strategy;
    delete[] strategySum;
    delete[] averageStrategy;
}

const double *Node::getStrategy() {
    return strategy;
}

const double *Node::getAverageStrategy() {
        calcAverageStrategy();
    return averageStrategy;
}

int Node::getActionNum() const {
    return actionNum;
}


void Node::calcAverageStrategy() {
    // if average strategy has already been calculated, do nothing to reduce the calculation time
    if (!updateStrategyFlag) {
        return;
    }

    // calculate average strategy
    for (int a = 0; a < actionNum; ++a) {
        averageStrategy[a] = 0.0;
    }
    double normalizingSum = 0.0;
    for (int a = 0; a < actionNum; ++a) {
        normalizingSum += strategySum[a];
    }
    for (int a = 0; a < actionNum; ++a) {
        if (normalizingSum > 0) {
            averageStrategy[a] = strategySum[a] / normalizingSum;
        } else {
            averageStrategy[a] = 1.0 / (double) actionNum;
        }
    }
    updateStrategyFlag = false;
}