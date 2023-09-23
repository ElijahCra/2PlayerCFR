//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_REGRETMINIMIZER_HPP
#define INC_2PLAYERCFR_REGRETMINIMIZER_HPP

#include <random>
#include "../Game/Game.hpp"
#include <unordered_map>
#include "Node.hpp"
#include "../Utility/Utility.hpp"


class RegretMinimizer {
public:
    explicit RegretMinimizer(uint32_t seed = std::random_device()());

    ~RegretMinimizer();

    void Train(int iterations);

    /// @brief recursively traverse game tree (depth-first) sampling only one chance outcome at each chance node and all actions
    /// @param updatePlayer player whos strategy is updated and utilities are retrieved in terms of
    /// @param probP0 probability of reaching current history given only P0 (bb) players contribution from all previous histories and chance ie. only the action nodes where p0 is acting
    /// @param probP1 probability of reaching current history given only P1 (sb) players contribution from all previous histories and chance
    /// @param probChance probability of reaching current history given only the chance contributions
    double ChanceCFR(const Game& game, int updatePlayer, double probP0, double probP1);

private:
    Utility util;

    std::mt19937 mRNG;

    std::unordered_map<std::string, Node* > mNodeMap;

    Game *mGame;

    uint64_t mNodeCount;

};


#endif //INC_2PLAYERCFR_REGRETMINIMIZER_HPP
