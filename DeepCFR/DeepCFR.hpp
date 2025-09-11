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
#include "CoroutineTraversal.hpp"

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

    void TrainCoro(uint32_t iterations);


private:
    /// @brief The recursive CFR traversal function.
    float traverse_cfr(const GameType& game, int updatePlayer, int current_iter, float probUpdatePlayer);

    TraversalTask<GameType> traverse_cfr_coro(GameType game, int update_player, int iteration, float prob_update_player);
    void initCoro();

    void run_traversals_for_player(int player, int iteration, int num_traversals);
    /// @brief Trains the advantage network from scratch using data from its replay buffer.
    void train_advantage_network(int player);

    /// @brief Trains the strategy network using data from its replay buffer.
    void train_strategy_network();

    void train_from_hybrid_storage();

    /// @brief Compute strategy using regret matching
    std::vector<float> compute_strategy_from_advantages(const std::vector<float>& advantages);

    /// @brief Add sample to memory with reservoir sampling
    template<typename T>
    void add_to_strategy_memory(std::vector<T>& memory, const T& sample, size_t max_size);

    std::mt19937 m_rng;
    GameType m_game;
    torch::Device m_device;


    HybridAdvantageStorage<GameType>::Config m_storage_config;
    std::array<std::unique_ptr<HybridAdvantageStorage<GameType>>, 2> m_hybrid_storage;

    GPUBatchProcessor<GameType> m_gpu_processor;

    // Neural Networks for advantage (regret) and strategy
    std::array<DeepCFRModel, 2> m_advantage_networks{nullptr, nullptr};
    DeepCFRModel m_strategy_network{nullptr};

    // Optimizers
    std::vector<torch::optim::Adam> m_advantage_optimizers{};
    torch::optim::Adam m_strategy_optimizer;

    // Replay Buffers
    std::array<AdvantageMemoryBuffer<GameType>, 2> m_adv_memories;
    std::vector<TrainingSampleStrategy> m_strategy_memory;

    uint32_t m_nodes_touched{};
    uint32_t m_gpu_nodes_touched{};

    // Training constants from the paper
    static constexpr size_t BATCH_SIZE = 10000;
    static constexpr size_t MEMORY_SIZE = 40000000; // 40 million
    static constexpr float LEARNING_RATE = 0.001f;
    static constexpr int SGD_ITERATIONS = 4000; // SGD iterations per training step
    static constexpr double GRADIENT_CLIP_NORM = 1.0;
    static constexpr int K_TRAVERSALS = 10000; // Number of traversals per iteration


    static HybridAdvantageStorage<GameType>::Config createStorageConfig() {
        typename HybridAdvantageStorage<GameType>::Config config;
        config.in_memory_capacity = 2000000;  // 2M in memory
        config.flush_batch_size = 50000;      // Flush every 50k samples
        config.max_db_size = 100000000;       // 100M max on disk
        config.flush_interval = std::chrono::milliseconds(10000);  // Flush every 10s
        config.num_flush_threads = 4;         // 4 background flush threads
        config.db_path = "./deep_cfr_advantage_db";
        return config;
    }

    HybridAdvantageStorage<GameType>* get_storage(int player) {
        return m_hybrid_storage[player].get();
    }
};

#endif //DEEPCFR_HPP
