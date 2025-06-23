

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
        explicit Node(uint8_t actionNum);

        void calcUpdatedStrategy();

        void calcAverageStrategy();

        [[nodiscard]] const std::vector<float> & getAverageStrategy() const;

        [[nodiscard]] std::vector<float> getStrategy() const;

        [[nodiscard]] const std::vector<float> & getRegretSum() const;

        [[nodiscard]] const std::vector<float> & getStrategySum() const;

        void setRegretSum(const std::vector<float>& regretSum);

        void setStrategySum(const std::vector<float>& strategySum);

        void setAverageStrategy(const std::vector<float>& averageStrategy);

        void updateRegretSum(int i, float actionRegret, float probCounterFactual);

        void updateStrategySum(const std::vector<float> &currentStrategy, float probUpdatePlayer);

    private:
        std::vector<float> regretSum;
        std::vector<float> strategy;
        std::vector<float> strategySum;
        std::vector<float> averageStrategy;
        uint8_t actionNum;
        mutable std::mutex mutex;
    };
}
#endif //INC_2PLAYERCFR_NODE_HPP
