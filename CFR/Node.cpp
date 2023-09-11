//
// Created by elijah on 8/29/2023.
//

#include "Node.hpp"

Node::Node(const int actionNum) : mActionNum(actionNum), mAlreadyCalculated(false), mNeedToUpdateStrategy(false) {
    mCumRegret = new double[actionNum];
    mStrategy = new double[actionNum];
    mCumStrategy = new double[actionNum];
    mAverageStrategy = new double[actionNum];
    for (int a = 0; a < actionNum; ++a) {
        mCumRegret[a] = 0.0;
        mStrategy[a] = 1.0 / (double) actionNum;
        mCumStrategy[a] = 0.0;
        mAverageStrategy[a] = 0.0;
    }
}

Node::~Node() {
    delete[] mCumRegret;
    delete[] mStrategy;
    delete[] mCumStrategy;
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
        mStrategy[a] = mCumRegret[a] > 0 ? mCumRegret[a] : 0;
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
        mCumStrategy[a] += realizationWeight * strategy[a];
    }
    mAlreadyCalculated = false;
}


double Node::regretSum(const int action) const {
    return mCumRegret[action];
}


void Node::regretSum(const int action, const double value) {
    mCumRegret[action] = value;
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
        normalizingSum += mCumStrategy[a];
    }
    for (int a = 0; a < mActionNum; ++a) {
        if (normalizingSum > 0) {
            mAverageStrategy[a] = mCumStrategy[a] / normalizingSum;
        } else {
            mAverageStrategy[a] = 1.0 / (double) mActionNum;
        }
    }
    mAlreadyCalculated = true;
}