

#ifndef INC_2PLAYERCFR_NODE_HPP
#define INC_2PLAYERCFR_NODE_HPP

#include <vector>
#include <cstdint>

namespace CFR {
/// @class Node
/// @brief Information set node class definition
    class Node {
    public:
        /// @param actionNum Number of available actions in this node
        explicit Node(int actionNum = 0);

        ~Node();

        /// @brief Get the current getStrategy at this node through Regret-Matching
        /// @return mixed getStrategy
        const float *getStrategy();

        /// @brief Get the average getStrategy across all training iterations
        /// @return average mixed getStrategy
        const float *averageStrategy();

        /// @brief Update the average strategy by doing addition the current getStrategy weighted by the contribution of the acting player at the this node.
        ///        The contribution is the probability of reaching this node if all players other than the acting player always choose actions leading to this node.
        /// @param strategy current getStrategy
        /// @param realizationWeight contribution of the acting player at this node
        void strategySum(const float *strategy, float realizationWeight);

        /// @brief Update current getStrategy
        void updateStrategy();

        /// @brief Get the cumulative counterfactual regret of the specified action
        /// @param action action
        /// @return cumulative counterfactual regret
        float regretSum(int action) const;

        /// @brief Update the cumulative counterfactual regret by doing addition the counterfactual regret weighted by the contribution of the all players other than the acting player at this node.
        /// @param action action
        /// @param value counterfactual regret
        void regretSum(int action, float value);

        /// @brief Get the number of available actions at this node.
        /// @return number of available actions
        uint8_t actionNum() const;

    private:


        /// @brief Calculate the average getStrategy across all training iterations
        void calcAverageStrategy();


        uint8_t mActionNum;
        float *mRegretSum;
        float *mStrategy;
        float *mStrategySum;
        float *mAverageStrategy;
        bool mAlreadyCalculated;
        bool mNeedToUpdateStrategy;
    };
}
#endif //INC_2PLAYERCFR_NODE_HPP
