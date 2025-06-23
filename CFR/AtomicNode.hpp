//
// Created by Elijah Crain on 6/22/25.
//

#ifndef ATOMICNODE_HPP
#define ATOMICNODE_HPP
#include <atomic>
#include <vector>
#include <cstdint>

namespace CFR {
/// @class AtomicNode
/// @brief Thread-safe information set node class using atomic operations
class AtomicNode {
public:
    /// @param actionNum allowable actions at this node
    explicit AtomicNode(uint8_t actionNum);

    void calcUpdatedStrategy();

    void calcAverageStrategy();

    [[nodiscard]] const std::vector<float> & getAverageStrategy() const;

    [[nodiscard]] const std::vector<float> & getStrategy() const;

    [[nodiscard]] const std::vector<float> & getRegretSum() const;

    [[nodiscard]] const std::vector<float> & getStrategySum() const;

    void setRegretSum(const std::vector<float>& regretSum);

    void setStrategySum(const std::vector<float>& strategySum);

    void setAverageStrategy(const std::vector<float>& averageStrategy);

    void updateRegretSum(int i, float actionRegret, float probCounterFactual);

    void updateStrategySum(const std::vector<float>& currentStrategy, float probUpdatePlayer);

private:
    std::vector<std::atomic<float>> regretSum;
    std::vector<std::atomic<float>> strategy;
    std::vector<std::atomic<float>> strategySum;
    std::vector<std::atomic<float>> averageStrategy;
    uint8_t actionNum;
};
}
#endif //ATOMICNODE_HPP
