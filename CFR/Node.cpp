

#include "Node.hpp"

namespace CFR {

    Node::Node(int actionNum) : actionNum(actionNum) {

        regretSum = new float[actionNum];
        strategy = new float[actionNum];
        strategySum = new float[actionNum];
        averageStrategy= new float[actionNum];

        for (int i=0; i<actionNum; ++i) {
            strategy[i] = 1.f / (float) actionNum;
            regretSum[i] = 0.f;
            strategySum[i] = 0.f;
            averageStrategy[i] = 0.f;
        }
    }

    Node::~Node() {
        delete[] strategy;
        delete[] strategySum;
        delete[] regretSum;
        delete[] averageStrategy;
    }

    void Node::calcUpdatedStrategy() {
        /*if (!updateThisStrategy) {
            return;
        }*/
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

    const float *Node::getStrategy() const {
        return strategy;
    }

    const float *Node::getRegretSum() const {
        return regretSum;
    }

    const float *Node::getAverageStrategy() const {
        return averageStrategy;
    }



    void Node::updateRegretSum(int i, float actionRegret, float probCounterFactual) {
        regretSum[i] += probCounterFactual * actionRegret;
    }

    void Node::updateStrategySum(const float *currentStrategy, float probUpdatePlayer) {
        for (int i=0; i<actionNum; ++i) {
            strategySum[i] += probUpdatePlayer * currentStrategy[i];
        }
    }

}
