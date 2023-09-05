//
// Created by elijah on 8/29/2023.
//

#include "Node.hpp"

Node::Node(const int actionNum) : mActionNum(actionNum), mAlreadyCalculated(false), mNeedToUpdateStrategy(false) {
    mRegretSum = new double[actionNum];
    mStrategy = new double[actionNum];
    mStrategySum = new double[actionNum];
    mAverageStrategy = new double[actionNum];
    for (int a = 0; a < actionNum; ++a) {
        mRegretSum[a] = 0.0;
        mStrategy[a] = 1.0 / (double) actionNum;
        mStrategySum[a] = 0.0;
        mAverageStrategy[a] = 0.0;
    }
}

Node::~Node() {
    delete[] mRegretSum;
    delete[] mStrategy;
    delete[] mStrategySum;
    delete[] mAverageStrategy;
}

const double *Node::strategy() {
    return mStrategy;
}

const double *Node::averageStrategy() {
    if (!mAlreadyCalculated) {
        calcAverageStrategy();
    }
    return mAverageStrategy;
}

void Node::updateStrategy() {
    if (!mNeedToUpdateStrategy) {
        return;
    }
    double normalizingSum = 0.0;
    for (int a = 0; a < mActionNum; ++a) {
        mStrategy[a] = mRegretSum[a] > 0 ? mRegretSum[a] : 0;
        normalizingSum += mStrategy[a];
    }
    for (int a = 0; a < mActionNum; ++a) {
        if (normalizingSum > 0) {
            mStrategy[a] /= normalizingSum;
        } else {
            mStrategy[a] = 1.0 / (double) mActionNum;
        }
    }
}

void Node::strategySum(const double *strategy, const double realizationWeight) {
    for (int a = 0; a < mActionNum; ++a) {
        mStrategySum[a] += realizationWeight * strategy[a];
    }
    mAlreadyCalculated = false;
}


double Node::regretSum(const int action) const {
    return mRegretSum[action];
}


void Node::regretSum(const int action, const double value) {
    mRegretSum[action] = value;
    mNeedToUpdateStrategy = true;
}


uint8_t Node::actionNum() const {
    return mActionNum;
}

/// @brief Calculate the average strategy across all training iterations
void Node::calcAverageStrategy() {
    // if average strategy has already been calculated, do nothing to reduce the calculation time
    if (mAlreadyCalculated) {
        return;
    }

    // calculate average strategy
    for (int a = 0; a < mActionNum; ++a) {
        mAverageStrategy[a] = 0.0;
    }
    double normalizingSum = 0.0;
    for (int a = 0; a < mActionNum; ++a) {
        normalizingSum += mStrategySum[a];
    }
    for (int a = 0; a < mActionNum; ++a) {
        if (normalizingSum > 0) {
            mAverageStrategy[a] = mStrategySum[a] / normalizingSum;
        } else {
            mAverageStrategy[a] = 1.0 / (double) mActionNum;
        }
    }
    mAlreadyCalculated = true;
}