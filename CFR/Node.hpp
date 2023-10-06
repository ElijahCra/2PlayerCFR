

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

        float* calcStrategy(float realizationWeight);

        float *calcAverageStrategy();


        /*

        ~Node();


        const float *getStrategy();


        const float *getAverageStrategy();


        void updateStrategySum(const float *strategy, float realizationWeight);


        void updateStrategy();


        float getRegretSum(int action) const;


        void updateRegretSum(int action, float value);

        uint8_t getActionNum() const;

        */



        const float *getStrategy();

        const float *getRegretSum();

        void updateRegretSum(int i, const float regret);

    private:

        float *regretSum;
        float *strategy;
        float *strategySum;
        float * averageStrategy;

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
