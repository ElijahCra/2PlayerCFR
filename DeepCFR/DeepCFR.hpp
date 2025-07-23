//
// Created by elijah on 7/16/25.
//

#ifndef DEEPCFR_HPP
#define DEEPCFR_HPP
#include <cstdint>
#include <memory>
#include <random>
#include <regex>
#include <vector>

#include "Net.hpp"
#include "torch/torch.h"
#include "../Game/Preflop/Game.hpp"
#include "../Game/Texas/Game.hpp"
#include "../Game/Utility/Utility.hpp"


struct InfoSet
{
    std::vector<torch::Tensor> cardTensors;
    torch::Tensor betTensor;

    [[nodiscard]] std::vector<torch::Tensor> getCardTensors() const { return cardTensors; }
    [[nodiscard]] torch::Tensor getBetTensor() const { return betTensor; }
};

// For advantage Memory
struct TrainingSampleAdvantage {
    InfoSet infoset;
    int iteration;
    std::vector<float> advantages;  // r_tilde(I, a) for each action
    std::vector<int> legal_action_indices;
    float weight;                   // iteration weight for Linear CFR
};

//For strategy memory
struct TrainingSampleStrategy
{
    InfoSet infoset;
    int iteration;
    std::vector<float> strategy;    // sigma(I, a) for each action
    float weight;                   // iteration weight for Linear CFR
};

template<typename GameType>
class DeepRegretMinimizer {
public:
    /// @brief Constructor initializes networks, optimizers, and the game engine.
    explicit DeepRegretMinimizer(uint32_t seed = std::random_device()());

    ~DeepRegretMinimizer()
    {
        torch::save(m_strategy_network,"stratmodel.pt");
    }

    /// @brief Main training loop.
    /// @param iterations The total number of game traversals to perform.
    void Train(uint32_t iterations);

private:
    /// @brief The recursive CFR traversal function.
    float traverse_cfr(const GameType& game, int updatePlayer, int current_iter, float p0, float p1);

    /// @brief Trains the advantage network from scratch using data from its replay buffer.
    void train_advantage_network(int player);

    /// @brief Trains the strategy network using data from its replay buffer.
    void train_strategy_network();

    /// @brief Compute strategy using regret matching
    std::vector<float> compute_strategy_from_advantages(const std::vector<float>& advantages);

    /// @brief Add sample to memory with reservoir sampling
    template<typename T>
    void add_to_memory(std::vector<T>& memory, const T& sample, size_t max_size);

    std::mt19937 m_rng;
    GameType m_game;

    // Neural Networks for advantage (regret) and strategy
    std::array<DeepCFRModel, 2> m_advantage_networks{nullptr, nullptr};
    DeepCFRModel m_strategy_network{nullptr};

    // Optimizers
    std::vector<torch::optim::Adam> m_advantage_optimizers{};
    torch::optim::Adam m_strategy_optimizer;

    // Replay Buffers
    std::array<std::vector<TrainingSampleAdvantage>, 2> m_adv_memories;
    std::vector<TrainingSampleStrategy> m_strategy_memory;

    // Training constants from the paper
    static constexpr size_t BATCH_SIZE = 10000;
    static constexpr size_t MEMORY_SIZE = 40000000; // 40 million
    static constexpr float LEARNING_RATE = 0.001f;
    static constexpr int SGD_ITERATIONS = 4000; // SGD iterations per training step
    static constexpr double GRADIENT_CLIP_NORM = 1.0;
    static constexpr int K_TRAVERSALS = 10000; // Number of traversals per iteration
};

#endif //DEEPCFR_HPP
