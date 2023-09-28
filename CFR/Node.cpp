

#include "Node.hpp"

namespace CFR {
/// @param actionNum Number of available actions in this node
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

/// @brief Get the current getStrategy at this node through Regret-Matching
/// @return mixed getStrategy
    const double *Node::getStrategy() {
        return mStrategy;
    }

/// @brief Get the average getStrategy across all training iterations
/// @return average mixed getStrategy
    const double *Node::averageStrategy() {
        if (!mAlreadyCalculated) {
            calcAverageStrategy();
        }
        return mAverageStrategy;
    }

/// @brief Update the average strategy by doing addition the current getStrategy weighted by the contribution of the acting player at the this node.
///        The contribution is the probability of reaching this node if all players other than the acting player always choose actions leading to this node.
/// @param strategy current getStrategy
/// @param realizationWeight contribution of the acting player at this node
    void Node::strategySum(const double *strategy, const double realizationWeight) {
        for (int a = 0; a < mActionNum; ++a) {
            mStrategySum[a] += realizationWeight * strategy[a];
        }
        mAlreadyCalculated = false;
    }

/// @brief Update current getStrategy
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

/// @brief Get the cumulative counterfactual regret of the specified action
/// @param action action
/// @return cumulative counterfactual regret
    double Node::regretSum(const int action) const {
        return mRegretSum[action];
    }

/// @brief Update the cumulative counterfactual regret by doing addition the counterfactual regret weighted by the contribution of the all players other than the acting player at this node.
/// @param action action
/// @param value counterfactual regret
    void Node::regretSum(const int action, const double value) {
        mRegretSum[action] = value;
        mNeedToUpdateStrategy = true;
    }

/// @brief Get the number of available actions at this node.
/// @return number of available actions
    uint8_t Node::actionNum() const {
        return mActionNum;
    }

/// @brief Calculate the average getStrategy across all training iterations
    void Node::calcAverageStrategy() {
        // if average getStrategy has already been calculated, do nothing to reduce the calculation time
        if (mAlreadyCalculated) {
            return;
        }

        // calculate average getStrategy
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
}
