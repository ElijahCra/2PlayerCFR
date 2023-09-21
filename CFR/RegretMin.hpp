//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_REGRETMIN_HPP
#define INC_2PLAYERCFR_REGRETMIN_HPP

#include <random>
#include "../Game/Game.hpp"
#include <unordered_map>
#include "Node.hpp"
#include "../Utility/Utility.hpp"


class RegretMin {
public:
    explicit RegretMin(uint32_t seed = std::random_device()());

    ~RegretMin();

    void Train(int iterations);

    /// @brief recursively traverse game tree (depth-first) sampling only one chance outcome at each chance node and all actions
    /// @param probP0 probability of reaching current history given only P0 (bb) players contribution from all previous histories ie. only the action nodes where p0 is acting
    /// @param probP1 probability of reaching current history given only P1 (sb) players contribution from all previous histories
    /// @param probChance probability of reaching current history given only the chance contributions
    double ChanceCFR(const Game& game, int playerNum, double probP0, double probP1, double probChance);

private:
    Utility util;

    std::mt19937 mRNG;

    std::unordered_map<std::string, Node* > mNodeMap;

    Game *mGame;

    uint64_t mNodeCount;

};


#endif //INC_2PLAYERCFR_REGRETMIN_HPP
