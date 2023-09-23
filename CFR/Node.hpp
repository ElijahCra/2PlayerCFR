//
// Copyright (c) 2020 Kenshi Abe
//

#ifndef REGRETMINIMIZATION_NODE_HPP
#define REGRETMINIMIZATION_NODE_HPP

#include <vector>



/// @class Node
/// @brief Information set node class definition
class Node {
public:
    /// @param actionNum Number of available actions in this node
    explicit Node(int actionNum = 0);

    ~Node();

    /// @brief Get the current getStrategy at this node through Regret-Matching
    /// @return mixed getStrategy
    const double *getStrategy();

    /// @brief Get the average getStrategy across all training iterations
    /// @return average mixed getStrategy
    const double *averageStrategy();

    /// @brief Update the average strategy by doing addition the current getStrategy weighted by the contribution of the acting player at the this node.
    ///        The contribution is the probability of reaching this node if all players other than the acting player always choose actions leading to this node.
    /// @param strategy current getStrategy
    /// @param realizationWeight contribution of the acting player at this node
    void strategySum(const double *strategy, double realizationWeight);

    /// @brief Update current getStrategy
    void updateStrategy();

    /// @brief Get the cumulative counterfactual regret of the specified action
    /// @param action action
    /// @return cumulative counterfactual regret
    double regretSum(int action) const;

    /// @brief Update the cumulative counterfactual regret by doing addition the counterfactual regret weighted by the contribution of the all players other than the acting player at this node.
    /// @param action action
    /// @param value counterfactual regret
    void regretSum(int action, double value);

    /// @brief Get the number of available actions at this node.
    /// @return number of available actions
    uint8_t actionNum() const;

private:
    friend class boost::serialization::access;

    /// @brief Calculate the average getStrategy across all training iterations
    void calcAverageStrategy();

    template<class Archive>
    void save(Archive &ar, const unsigned int version) const {
        std::vector<double> vec(mAverageStrategy, mAverageStrategy + mActionNum);
        ar & vec;
    }

    template<class Archive>
    void load(Archive &ar, const unsigned int version) {
        std::vector<double> vec;
        ar & vec;
        mActionNum = vec.size();
        delete[] mAverageStrategy;
        mAverageStrategy = new double[vec.size()];
        for (int i = 0; i < vec.size(); ++i) {
            mAverageStrategy[i] = vec[i];
        }
        mAlreadyCalculated = true;
        mNeedToUpdateStrategy = false;
    }



    uint8_t mActionNum;
    double *mRegretSum;
    double *mStrategy;
    double *mStrategySum;
    double *mAverageStrategy;
    bool mAlreadyCalculated;
    bool mNeedToUpdateStrategy;
};

#endif //REGRETMINIMIZATION_NODE_HPP
