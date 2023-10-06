

#include "Node.hpp"

namespace CFR {

    Node::Node(int actionNum) : actionNum(actionNum) {

        regretSum = new float[actionNum];
        strategy = new float[actionNum];
        strategySum = new float[actionNum];
        averageStrategy= new float[actionNum];

    }

    float *Node::calcStrategy(float realizationWeight) {
        float normalizingSum = 0;
        for (int a = 0; a < actionNum; a++) {
            strategy[a] = regretSum[a] > 0 ? regretSum[a] : 0;
            normalizingSum += strategy[a];
        }
        for (int a = 0; a < actionNum; a++) {
            if (normalizingSum > 0)
                strategy[a] /= normalizingSum;
            else
                strategy[a] = 1.f / (float)actionNum;
            strategySum[a] += realizationWeight * strategy[a];
        }
        return strategy;
    }

    float *Node::calcAverageStrategy() {
        float normalizingSum = 0;
        for (int a = 0; a < actionNum; a++)
            normalizingSum += strategySum[a];
        for (int a = 0; a < actionNum; a++)
            if (normalizingSum > 0)
                averageStrategy[a] = strategySum[a] / normalizingSum;
            else
                averageStrategy[a] = 1.f / (float)actionNum;
        return averageStrategy;
    }

    const float *Node::getStrategy() {
        return strategy;
    }

    const float *Node::getRegretSum() {
        return regretSum;
    }

    void Node::updateRegretSum(int i, const float regret) {
        regretSum[i] = regret;
    }
    /*

    Node::Node(const int actionNum) : actionNum(actionNum), alreadyCalculated(false), needToUpdateStrategy(false) {
        regretSum = new float[actionNum] {0.f};
        strategy = new float[actionNum] {0.f};
        strategySum = new float[actionNum] {0.f};
        averageStrategy = new float[actionNum] {0.f};
        for (int a = 0; a < actionNum; ++a) {
            regretSum[a] = 0.f;
            strategy[a] = 1.f / (float) actionNum;
            strategySum[a] = 0.f;
            averageStrategy[a] = 0.f;
        }
    }

    Node::~Node() {
        delete[] regretSum;
        delete[] strategy;
        delete[] strategySum;
        delete[] averageStrategy;
    }

    const float *Node::getStrategy() {
        return strategy;
    }

    const float *Node::getAverageStrategy() {
        if (!alreadyCalculated) {
            calcAverageStrategy();
        }
        return averageStrategy;
    }


    void Node::updateStrategySum(const float *strategy, const float realizationWeight) {
        for (int a = 0; a < actionNum; ++a) {
            strategySum[a] += realizationWeight * strategy[a];
        }
        alreadyCalculated = false;
    }


    void Node::updateStrategy() {
        if (!needToUpdateStrategy) {
            return;
        }
        float normalizingSum = 0.0;
        for (int a = 0; a < actionNum; ++a) {
            strategy[a] = regretSum[a] > 0 ? regretSum[a] : 0;
            normalizingSum += strategy[a];
        }
        for (int a = 0; a < actionNum; ++a) {
            if (normalizingSum > 0) {
                strategy[a] /= normalizingSum;
            } else {
                strategy[a] = 1.f / (float) actionNum;
            }
        }
    }


    float Node::getRegretSum(const int action) const {
        return regretSum[action];
    }


    void Node::updateRegretSum(const int action, const float value) {
        regretSum[action] = value;
        needToUpdateStrategy = true;
    }

    uint8_t Node::getActionNum() const {
        return actionNum;
    }


    void Node::calcAverageStrategy() {
        // if average getStrategy has already been calculated, do nothing to reduce the calculation time
        if (alreadyCalculated) {
            return;
        }

        // calculate average getStrategy
        for (int a = 0; a < actionNum; ++a) {
            averageStrategy[a] = 0.0;
        }
        float normalizingSum = 0.0;
        for (int a = 0; a < actionNum; ++a) {
            normalizingSum += strategySum[a];
        }
        for (int a = 0; a < actionNum; ++a) {
            if (normalizingSum > 0) {
                averageStrategy[a] = strategySum[a] / normalizingSum;
            } else {
                averageStrategy[a] = 1.0 / (float) actionNum;
            }
        }
        alreadyCalculated = true;
    }
     */
}
