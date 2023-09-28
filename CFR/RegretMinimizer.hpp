//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_REGRETMINIMIZER_HPP
#define INC_2PLAYERCFR_REGRETMINIMIZER_HPP

#include <random>
#include "../Game/Preflop/Game.hpp"
#include "../Game/Texas/Game.hpp"
#include <unordered_map>
#include "Node.hpp"
#include "../Utility/Utility.hpp"

namespace CFR {

    template<typename GameType>
    class RegretMinimizer {
    public:
        explicit RegretMinimizer(uint32_t seed = std::random_device()());

        ~RegretMinimizer();


        void Train(int iterations);

        /// @brief recursively traverse game tree (depth-first) sampling only one chance outcome at each chance node and all actions
        /// @param updatePlayer player whos getStrategy is updated and utilities are retrieved in terms of
        /// @param probCounterFactual reach contribution of all players and chance except for active player (who is the update player for this implementation)
        /// @param probUpdatePlayer reach contribution of only the active player (update player)
        /// @param probChance probability of reaching current history given only the chance contributions
        double ChanceCFR(const GameType &game, int updatePlayer, double probCounterFactual, double probUpdatePlayer);

    private:
        Utility util;

        std::mt19937 mRNG;

        std::unordered_map<std::string, Node *> mNodeMap;

        GameType *mGame;

        uint64_t mNodeCount;

    };

}
#endif //INC_2PLAYERCFR_REGRETMINIMIZER_HPP
