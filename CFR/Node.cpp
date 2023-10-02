

#include "Node.hpp"

namespace CFR {
/// @param actionNum Number of available actions in this node
    Node::Node(const int actionNum) : mActionNum(actionNum), mAlreadyCalculated(false), mNeedToUpdateStrategy(false) {
        mRegretSum = new float[actionNum] {0.f};
        mStrategy = new float[actionNum] {0.f};
        mStrategySum = new float[actionNum] {0.f};
        mAverageStrategy = new float[actionNum] {0.f};
        for (int a = 0; a < actionNum; ++a) {
            mRegretSum[a] = 0.f;
            mStrategy[a] = 1.f / (float) actionNum;
            mStrategySum[a] = 0.f;
            mAverageStrategy[a] = 0.f;
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
    const float *Node::getStrategy() {
        return mStrategy;
    }

/// @brief Get the average getStrategy across all training iterations
/// @return average mixed getStrategy
    const float *Node::averageStrategy() {
        if (!mAlreadyCalculated) {
            calcAverageStrategy();
        }
        return mAverageStrategy;
    }

/// @brief Update the average strategy by doing addition the current getStrategy weighted by the contribution of the acting player at the this node.
///        The contribution is the probability of reaching this node if all players other than the acting player always choose actions leading to this node.
/// @param strategy current getStrategy
/// @param realizationWeight contribution of the acting player at this node
    void Node::strategySum(const float *strategy, const float realizationWeight) {
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
        float normalizingSum = 0.0;
        for (int a = 0; a < mActionNum; ++a) {
            mStrategy[a] = mRegretSum[a] > 0 ? mRegretSum[a] : 0;
            normalizingSum += mStrategy[a];
        }
        for (int a = 0; a < mActionNum; ++a) {
            if (normalizingSum > 0) {
                mStrategy[a] /= normalizingSum;
            } else {
                mStrategy[a] = 1.f / (float) mActionNum;
            }
        }
    }

/// @brief Get the cumulative counterfactual regret of the specified action
/// @param action action
/// @return cumulative counterfactual regret
    float Node::regretSum(const int action) const {
        return mRegretSum[action];
    }

/// @brief Update the cumulative counterfactual regret by doing addition the counterfactual regret weighted by the contribution of the all players other than the acting player at this node.
/// @param action action
/// @param value counterfactual regret
    void Node::regretSum(const int action, const float value) {
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
        float normalizingSum = 0.0;
        for (int a = 0; a < mActionNum; ++a) {
            normalizingSum += mStrategySum[a];
        }
        for (int a = 0; a < mActionNum; ++a) {
            if (normalizingSum > 0) {
                mAverageStrategy[a] = mStrategySum[a] / normalizingSum;
            } else {
                mAverageStrategy[a] = 1.0 / (float) mActionNum;
            }
        }
        mAlreadyCalculated = true;
    }
}
