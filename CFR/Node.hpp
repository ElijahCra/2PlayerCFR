

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

        void calcUpdatedStrategy();

        void calcAverageStrategy();

        const std::vector<float> &getAverageStrategy() const;

        const std::vector<float> &getStrategy() const;

        const std::vector<float> &getRegretSum() const;

        void updateRegretSum(int i, float actionRegret, float probCounterFactual);

        void updateStrategySum(const std::vector<float> &currentStrategy, float probUpdatePlayer);

    private:

        std::vector<float> regretSum;
        std::vector<float> strategy;
        std::vector<float> strategySum;
        std::vector<float> averageStrategy;

        //bool updateThisStrategy;
        uint8_t actionNum;

    };
}
#endif //INC_2PLAYERCFR_NODE_HPP
