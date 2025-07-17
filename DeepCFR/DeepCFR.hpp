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

/// @struct TrainingSample
/// @brief Stores a single experience tuple for training the neural networks.
/// This version is adapted for the new network architecture.
struct TrainingSample {
    // Inputs to the network
    std::vector<torch::Tensor> cards;
    torch::Tensor bets;

    // Targets for training
    torch::Tensor target_values; // Can be regrets or strategy probabilities
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

    /// @brief Trains the advantage network from scratch using data from its replay buffer.
    void train_advantage_network();

    /// @brief Trains the strategy network using data from its replay buffer.
    void train_strategy_network();

    std::mt19937 m_rng;
    GameType m_game;

    // Neural Networks for advantage (regret) and strategy
    std::shared_ptr<DeepCFRModelImpl> m_adv_network;
    std::shared_ptr<DeepCFRModelImpl> m_strategy_network;

    // Optimizers
    std::unique_ptr<torch::optim::Adam> m_adv_optimizer;
    std::unique_ptr<torch::optim::Adam> m_strategy_optimizer;

    // Replay Buffers
    std::vector<TrainingSample> m_adv_memory;
    std::vector<TrainingSample> m_strategy_memory;

    // Training constants from the paper
    static constexpr size_t BATCH_SIZE = 10000;
    static constexpr size_t MEMORY_SIZE = 40000000; // 40 million
    static constexpr float LEARNING_RATE = 0.001f;
    static constexpr int SGD_ITERATIONS = 4000; // SGD iterations per training step
    static constexpr double GRADIENT_CLIP_NORM = 1.0;
};


/// @class InfoSetConverter
/// @brief Converts string-based information sets into structured tensors for the new NN.
class InfoSetConverter {
public:
    // Constants for the network architecture and game structure
    static constexpr int64_t NUM_CARD_TYPES = 4;      // Hole, Flop, Turn, River
    static constexpr int64_t MAX_BETS = 12;           // Max bets in a history
    static constexpr int64_t NUM_ACTION_TYPES = 15;   // From game enum
    static constexpr int64_t CARD_INPUT_DIM = 2;      // num_cards per type (e.g., 2 hole cards)

    /// @brief Converts an info set string to a tuple of card and bet tensors.
    /// @param info_set_str The info set string from the game.
    /// Expected format: "H:Ac,Kd;B:5s,Th,Jc;A:c,r100,c"
    /// H: Hole Cards, B: Board Cards, A: Action History (c=check/call, r=raise, f=fold)
    /// @return A tuple containing {vector_of_card_tensors, bet_tensor}.
    static std::tuple<std::vector<torch::Tensor>, torch::Tensor> convert(const std::string& info_set_str) {
        std::vector<torch::Tensor> card_tensors;
        // Initialize tensors for Hole, Flop, Turn, River. -1 indicates no card.
        torch::Tensor hole_cards = torch::full({CARD_INPUT_DIM}, -1, torch::kLong);
        torch::Tensor flop_cards = torch::full({3}, -1, torch::kLong);
        torch::Tensor turn_card = torch::full({1}, -1, torch::kLong);
        torch::Tensor river_card = torch::full({1}, -1, torch::kLong);

        torch::Tensor bet_tensor = torch::zeros({MAX_BETS});

        std::regex section_re("([HBA]):([^;]+)");
        auto sections_begin = std::sregex_iterator(info_set_str.begin(), info_set_str.end(), section_re);
        auto sections_end = std::sregex_iterator();

        for (std::sregex_iterator i = sections_begin; i != sections_end; ++i) {
            std::smatch match = *i;
            std::string section_type = match[1].str();
            std::string section_data = match[2].str();

            if (section_type == "H" || section_type == "B") { // Cards
                std::regex card_re("([2-9TJQKA][cdhs])");
                auto cards_begin = std::sregex_iterator(section_data.begin(), section_data.end(), card_re);
                auto cards_end = std::sregex_iterator();
                int card_count = 0;
                for (std::sregex_iterator j = cards_begin; j != cards_end; ++j) {
                    int64_t card_idx = cardStringToIndex((*j).str());
                    if (section_type == "H" && card_count < 2) hole_cards[card_count++] = card_idx;
                    else if (section_type == "B") {
                        if (card_count < 3) flop_cards[card_count++] = card_idx;
                        else if (card_count == 3) turn_card[0] = card_idx;
                        else if (card_count == 4) river_card[0] = card_idx;
                    }
                }
            } else if (section_type == "A") { // Actions/Bets
                std::regex action_re("([cfr])(\\d*)");
                auto actions_begin = std::sregex_iterator(section_data.begin(), section_data.end(), action_re);
                auto actions_end = std::sregex_iterator();
                int bet_count = 0;
                for (std::sregex_iterator j = actions_begin; j != actions_end && bet_count < MAX_BETS; ++j) {
                    std::smatch action_match = *j;
                    float bet_value = 0.0f;
                    if (action_match[1].str() == "r" && !action_match[2].str().empty()) {
                        bet_value = std::stof(action_match[2].str());
                    }
                    bet_tensor[bet_count++] = bet_value;
                }
            }
        }

        // The network expects inputs as a batch, so we add a batch dimension of 1.
        card_tensors.push_back(hole_cards.unsqueeze(0));
        card_tensors.push_back(flop_cards.unsqueeze(0));
        card_tensors.push_back(turn_card.unsqueeze(0));
        card_tensors.push_back(river_card.unsqueeze(0));

        return std::make_tuple(card_tensors, bet_tensor.unsqueeze(0));
    }

private:
    static int64_t cardStringToIndex(const std::string& card_str) {
        if (card_str.length() != 2) return -1;
        char rank_char = card_str[0];
        char suit_char = card_str[1];
        int rank = -1, suit = -1;

        std::string ranks = "23456789TJQKA";
        std::string suits = "cdhs";

        size_t rank_pos = ranks.find(rank_char);
        if (rank_pos != std::string::npos) rank = rank_pos;

        size_t suit_pos = suits.find(suit_char);
        if (suit_pos != std::string::npos) suit = suit_pos;

        if (rank != -1 && suit != -1) return rank * 4 + suit;
        return -1; // Invalid card string
    }
};

#endif //DEEPCFR_HPP