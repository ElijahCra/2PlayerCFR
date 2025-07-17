//
// Created by elijah on 7/16/25.
//

#ifndef DEEPCFR_HPP
#define DEEPCFR_HPP
#include <cstdint>
#include <memory>
#include <random>
#include <regex>
#include <unordered_map>

#include "Net.hpp"
#include "torch/torch.h"
#include "../Game/GameImpl/Preflop/Game.hpp"
#include "../Game/GameImpl/Texas/Game.hpp"
#include "../Game/Utility/Utility.hpp"

/// @struct TrainingSample
/// @brief Stores a single experience tuple for training the neural networks.
struct TrainingSample {
    torch::Tensor info_set_tensor;
    torch::Tensor iteration_regrets;
    torch::Tensor iteration_strategy;
};


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

    /// @brief Trains the networks using data from the replay buffers.
    void update_networks();

    std::mt19937 m_rng;
    GameType m_game;

    // Neural Networks for advantage (regret) and strategy
    std::shared_ptr<DeepCFRModelImpl> m_adv_network;
    std::shared_ptr<DeepCFRModelImpl> m_strategy_network;

    // Optimizers
    std::unique_ptr<torch::optim::Adam> m_adv_optimizer{};
    std::unique_ptr<torch::optim::Adam> m_strategy_optimizer{};

    // Replay Buffers
    std::vector<TrainingSample> m_adv_memory;
    std::vector<TrainingSample> m_strategy_memory;

    // Training constants
    static constexpr size_t BATCH_SIZE = 256;
    static constexpr size_t MEMORY_SIZE = 100000;
    static constexpr float LEARNING_RATE = 0.001f;
};


/// @class InfoSetConverter
/// @brief Converts string-based information sets into fixed-size tensors for the NN.
class InfoSetConverter {
public:
    // Maximum number of actions in a history sequence. Padded if shorter.
    static constexpr int64_t MAX_ACTION_HISTORY = 12;
    // Number of possible actions, from the game's action enum.
    static constexpr int64_t NUM_ACTION_TYPES = 15;
    // Total input size for the neural network.
    // [Card Abstraction (1) + Public State (1) + Action History (MAX_ACTION_HISTORY * NUM_ACTION_TYPES)]
    static constexpr int64_t INPUT_SIZE = 1 + 1 + (MAX_ACTION_HISTORY * NUM_ACTION_TYPES);

    /// @brief Converts an info set string to a tensor.
    /// @param info_set_str The info set string from the game, e.g., "1024CaRaCh".
    /// @return A 1D tensor representing the featurized information set.
    static torch::Tensor convert(const std::string& info_set_str) {
        auto features = torch::zeros({INPUT_SIZE});

        // 1. Parse Card and Action History
        std::regex re("(\\d+)([a-zA-Z0-9]*)");
        std::smatch match;
        if (!std::regex_match(info_set_str, match, re) || match.size() != 3) {
             throw std::runtime_error("Invalid info set format: " + info_set_str);
        }

        // 2. Featurize Card Index
        // The first submatch is the full string, second is the number, third is the actions
        long long card_idx = std::stoll(match[1].str());
        features[0] = static_cast<float>(card_idx); // First feature is the card index

        // 3. Featurize Action History
        std::string actions_str = match[2].str();
        std::regex action_re("[A-Z][a-z0-9]?[0-9]?");
        auto actions_begin = std::sregex_iterator(actions_str.begin(), actions_str.end(), action_re);
        auto actions_end = std::sregex_iterator();

        int action_count = 0;
        for (std::sregex_iterator i = actions_begin; i != actions_end && action_count < MAX_ACTION_HISTORY; ++i) {
            std::string action_token = (*i).str();
            Preflop::GameBase::Action action = strToAction(action_token);
            int action_idx = static_cast<int>(action);
            if (action_idx >= 0 && action_idx < NUM_ACTION_TYPES) {
                // One-hot encode the action
                features[2 + action_count * NUM_ACTION_TYPES + action_idx] = 1.0f;
            }
            action_count++;
        }

        return features;
    }

private:
    /// @brief Maps an action string token to its corresponding enum value.
    static Preflop::GameBase::Action strToAction(const std::string& s) {
        static const std::unordered_map<std::string, Preflop::GameBase::Action> action_map = {
            {"Fo", Preflop::GameBase::Action::Fold}, {"Ch", Preflop::GameBase::Action::Check},
            {"Ca", Preflop::GameBase::Action::Call}, {"Ra1", Preflop::GameBase::Action::Raise1},
            {"Ra2", Preflop::GameBase::Action::Raise2}, {"Ra3", Preflop::GameBase::Action::Raise3},
            {"Ra5", Preflop::GameBase::Action::Raise5}, {"Ra10", Preflop::GameBase::Action::Raise10},
            {"Re2", Preflop::GameBase::Action::Reraise2}, {"Re4", Preflop::GameBase::Action::Reraise4},
            {"Re6", Preflop::GameBase::Action::Reraise6}, {"Re10", Preflop::GameBase::Action::Reraise10},
            {"Re20", Preflop::GameBase::Action::Reraise20}, {"AI", Preflop::GameBase::Action::AllIn}
        };
        auto it = action_map.find(s);
        return it != action_map.end() ? it->second : Preflop::GameBase::Action::None;
    }
};



#endif //DEEPCFR_HPP
