//
// Created by elijah on 7/16/25.
//

#include "DeepCFR.hpp"
#include <cassert>
#include <iostream>
#include <algorithm> // For std::shuffle

template<typename GameType>
DeepRegretMinimizer<GameType>::DeepRegretMinimizer(uint32_t seed)
    : m_rng(seed),
      m_game(m_rng)
{
    // Advantage network is initialized within the training loop.
    Utility::initLookup();
    for (int i =0; i<m_adv_memories.size(); ++i)
    {
        m_adv_memories[i].reserve(MEMORY_SIZE);
        m_strategy_memories[i].reserve(MEMORY_SIZE);
    }
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::Train(uint32_t iterations) {
    for (uint32_t i = 1; i <= iterations; ++i)
    {
        for (int p = 0; p < GameType::PlayerNum; ++p)
        {

        }
    }

}

template<typename GameType>
float DeepRegretMinimizer<GameType>::traverse_cfr(const GameType& game, int updatePlayer, int current_iter, float p0, float p1) {
    if (game.getType() == "terminal") {
        return game.getUtility(updatePlayer);
    }

    if (game.getType() == "chance") {
        GameType next_game = game;
        next_game.transition(GameType::Action::None); // Sample a chance event
        return traverse_cfr(next_game, updatePlayer, current_iter, p0, p1);
    }


}

template<typename GameType>
void DeepRegretMinimizer<GameType>::train_advantage_network() {

}

template<typename GameType>
void DeepRegretMinimizer<GameType>::train_strategy_network() {

}


// Explicit template instantiation
template class DeepRegretMinimizer<Preflop::Game>;
template class DeepRegretMinimizer<Texas::Game>;