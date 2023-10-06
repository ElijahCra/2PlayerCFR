

#ifndef INC_2PLAYERCFR_NODE_HPP
#define INC_2PLAYERCFR_NODE_HPP

#include <vector>
#include <cstdint>

namespace CFR {
/// @class Node
/// @brief Information set node class definition
    class Node {
    public:
        /// @param actionNum allowable actions at this node
        explicit Node(int actionNum);

        float* calcUpdatedStrategy(float realizationWeight);

        float *calcAverageStrategy();

        const float *getAverageStrategy() const;

        const float *getStrategy() const;

        const float *getRegretSum() const;

        void updateRegretSum(int i, float regret, float probCounterFactual);

        void updateStrategySum(const float *currentStrategy, float probUpdatePlayer);

    private:

        float *regretSum;
        float *strategy;
        float *strategySum;
        float * averageStrategy;

        bool updateThisStrategy;
        uint8_t actionNum;

        /*
        void calcAverageStrategy();
        uint8_t actionNum;
        float *regretSum;
        float *strategy;
        float *strategySum;
        float *averageStrategy;
        bool alreadyCalculated;
        bool needToUpdateStrategy;
         */
    };
}
#endif //INC_2PLAYERCFR_NODE_HPP
