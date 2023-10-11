

#include "Node.hpp"

namespace CFR {

    Node::Node(int actionNum) : actionNum(actionNum) {
        for (int i=0; i<actionNum; ++i) {
            strategy.push_back(1.f / (float) actionNum);
            regretSum.push_back(0.f);
            strategySum.push_back(0.f);
            averageStrategy.push_back(0.f);
        }
    }

    void Node::calcUpdatedStrategy() {
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
        }
    }

    void Node::calcAverageStrategy() {
        float normalizingSum = 0;
        for (int a = 0; a < actionNum; a++)
            normalizingSum += strategySum[a];
        for (int a = 0; a < actionNum; a++)
            if (normalizingSum > 0)
                averageStrategy[a] = strategySum[a] / normalizingSum;
            else
                averageStrategy[a] = 1.f / (float)actionNum;
    }

    const std::vector<float> &Node::getStrategy() const {
        return strategy;
    }

    const std::vector<float> &Node::getRegretSum() const {
        return regretSum;
    }

    const std::vector<float> &Node::getAverageStrategy() const {
        return averageStrategy;
    }


    void Node::updateRegretSum(int i, float actionRegret, float probCounterFactual) {
        regretSum[i] += probCounterFactual * actionRegret;
    }

    void Node::updateStrategySum(const std::vector<float> &currentStrategy, float probUpdatePlayer) {
        for (int i=0; i<actionNum; ++i) {
            strategySum[i] += probUpdatePlayer * currentStrategy[i];
        }
    }
}
