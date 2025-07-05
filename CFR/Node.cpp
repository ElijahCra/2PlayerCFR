

#include "Node.hpp"

namespace CFR {

    Node::Node(uint8_t actionNum) : actionNum(actionNum) {
        for (int i=0; i<actionNum; ++i) {
            strategy.push_back(1.F / static_cast<float>(actionNum));
            regretSum.push_back(0.F);
            strategySum.push_back(0.F);
            averageStrategy.push_back(0.F);
        }
    }

    void Node::calcUpdatedStrategy() {
        float normalizingSum = 0;
        for (int a = 0; a < actionNum; a++) {
            strategy[a] = regretSum[a] > 0 ? regretSum[a] : 0;
            normalizingSum += strategy[a];
        }
        for (int a = 0; a < actionNum; a++) {
            if (normalizingSum > 0) {
                strategy[a] /= normalizingSum;
            } else {
                strategy[a] = 1.F / static_cast<float>(actionNum);
            }
        }
    }

    void Node::calcAverageStrategy() {
        float normalizingSum = 0;
        for (int a = 0; a < actionNum; a++) {
            normalizingSum += strategySum[a];
        }
        for (int a = 0; a < actionNum; a++) {
            if (normalizingSum > 0) {
                averageStrategy[a] = strategySum[a] / normalizingSum;
            } else {
                averageStrategy[a] = 1.f / (float)actionNum;
            }
        }
    }

    auto Node::getStrategy() const -> const std::vector<float> & {
        return strategy;
    }

    auto Node::getRegretSum() const -> const std::vector<float> & {
        return regretSum;
    }

    auto Node::getAverageStrategy() const -> const std::vector<float> & {
        return averageStrategy;
    }

    auto Node::getStrategySum() const -> const std::vector<float> & {
        return strategySum;
    }

    void Node::setRegretSum(const std::vector<float>& regretSum) {
        this->regretSum = regretSum;
    }

    void Node::setStrategySum(const std::vector<float>& strategySum) {
        this->strategySum = strategySum;
    }

    void Node::setAverageStrategy(const std::vector<float>& averageStrategy) {
        this->averageStrategy = averageStrategy;
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
