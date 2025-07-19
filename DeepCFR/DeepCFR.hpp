//
// Created by elijah on 7/16/25.
//

#ifndef DEEPCFR_HPP
#define DEEPCFR_HPP
#include <cstdint>
#include <memory>
#include <random>
#include <regex>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>

#include "Net.hpp"
#include "torch/torch.h"
#include "../Game/GameImpl/Preflop/Game.hpp"
#include "../Game/GameImpl/Texas/Game.hpp"
#include "../Game/Utility/Utility.hpp"


template<typename GameType>
class DeepRegretMinimizer {
public:
    /// @brief Constructor initializes networks, optimizers, and the game engine.
    explicit DeepRegretMinimizer(uint32_t seed = std::random_device()());

    /// @brief Main training loop.
    /// @param iterations The total number of game traversals to perform.
    void Train(uint32_t iterations);

private:
    /// @brief The recursive CFR traversal function.
    float traverse_cfr(const GameType& game, int updatePlayer, int current_iter, float p0, float p1);

    /// @brief Trains the advantage network from scratch using data from its replay buffer.
    void train_advantage_network();

    /// @brief Trains the strategy network using data from its replay buffer.
    void train_strategy_network();

    std::mt19937 m_rng;
    GameType m_game;

    // Neural Networks for advantage (regret) and strategy - 1 or two for each?

    // Optimizers



    // Replay Buffers
    std::array<std::vector<TrainingSample>,2> m_adv_memories;
    std::array<std::vector<TrainingSample>,2> m_strategy_memories;

    // Training constants from the paper
    static constexpr size_t BATCH_SIZE = 10000;
    static constexpr size_t MEMORY_SIZE = 40000000; // 40 million
    static constexpr float LEARNING_RATE = 0.001f;
    static constexpr int SGD_ITERATIONS = 4000; // SGD iterations per training step
    static constexpr double GRADIENT_CLIP_NORM = 1.0;
};



#endif //DEEPCFR_HPP