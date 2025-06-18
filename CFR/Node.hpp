

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

        [[nodiscard]] auto getAverageStrategy() const -> const std::vector<float> &;

        [[nodiscard]] auto getStrategy() const -> const std::vector<float> &;

        [[nodiscard]] auto getRegretSum() const -> const std::vector<float> &;

        [[nodiscard]] auto getStrategySum() const -> const std::vector<float> &;

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
    };
}
#endif //INC_2PLAYERCFR_NODE_HPP
