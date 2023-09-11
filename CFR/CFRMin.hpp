//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_CFRMIN_HPP
#define INC_2PLAYERCFR_CFRMIN_HPP

#include <random>
#include "../Game/Game.hpp"
//#include "../Game/GameState.hpp"
#include <unordered_map>
#include "Node.hpp"


class CFRMin {
public:
    explicit CFRMin(const uint32_t seed = std::random_device()());

    ~CFRMin();

    void Train(int iterations);


private:

    /// @brief recursively traverse game tree (depth-first)
    /// @param probActingContribution probability of reaching current history given only the acting players contribution from all previous histories
    /// @param probNotActingContr probability of reaching current history given only non acting players contribution from all previous histories
    /// @param probChance probability of reaching current history given only the chance contributions
    double VanillaCFR(const Game& game, int playerNum, double probActing, double probNotActing, double probChance);



    std::mt19937 mRNG;

    std::unordered_map<std::string, Node *> mNodeMap;
    uint64_t mNodeTouchedCnt;
    Game *mGame;

    int mNodeCount;

};


#endif //INC_2PLAYERCFR_CFRMIN_HPP
