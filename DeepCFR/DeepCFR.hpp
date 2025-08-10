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
#include "types.hpp"
#include "AdvantageMemoryBuffer.hpp"
#include "GPUDispatcher.hpp"
#include "ThreadPool.hpp"

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

    void TrainParallel(uint32_t iterations, size_t num_threads = std::thread::hardware_concurrency());
private:
    /// @brief The recursive CFR traversal function.
    float traverse_cfr(const GameType& game, int updatePlayer, int current_iter, float probUpdatePlayer);

    std::future<float> traverse_cfr_coro(GameType game, int updatePlayer, int current_iter, ThreadPool& thread_pool, GPUDispatcher& dispatcher, std::mt19937& rng);
    /// @brief Trains the advantage network from scratch using data from its replay buffer.
    void train_advantage_network(int player);

    /// @brief Trains the strategy network using data from its replay buffer.
    void train_strategy_network();

    std::mt19937 m_rng;
    GameType m_game;
    torch::Device m_device;

    // Neural Networks for advantage (regret) and strategy
    std::array<DeepCFRModel, 2> m_advantage_networks{nullptr, nullptr};
    DeepCFRModel m_strategy_network{nullptr};

    // Optimizers
    std::vector<torch::optim::Adam> m_advantage_optimizers{};
    torch::optim::Adam m_strategy_optimizer;

    // Replay Buffers
    std::array<AdvantageMemoryBuffer<GameType>, 2> m_adv_memories;
    std::vector<TrainingSampleStrategy> m_strategy_memory;



};

#endif //DEEPCFR_HPP
