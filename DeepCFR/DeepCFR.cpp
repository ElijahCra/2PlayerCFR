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
      m_game(m_rng),
      m_strategy_network(std::make_shared<DeepCFRModelImpl>(InfoSetConverter::NUM_CARD_TYPES, InfoSetConverter::MAX_BETS, InfoSetConverter::NUM_ACTION_TYPES)),
      m_strategy_optimizer(std::make_unique<torch::optim::Adam>(m_strategy_network->parameters(), torch::optim::AdamOptions(LEARNING_RATE)))
{
    // Advantage network is initialized within the training loop.
    Utility::initLookup();
    m_adv_memory.reserve(MEMORY_SIZE);
    m_strategy_memory.reserve(MEMORY_SIZE);
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::Train(uint32_t iterations) {
    for (uint32_t i = 1; i <= iterations; ++i) {
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            // 1. Re-initialize player p's advantage network and optimizer from scratch
            m_adv_network = std::make_shared<DeepCFRModelImpl>(InfoSetConverter::NUM_CARD_TYPES, InfoSetConverter::MAX_BETS, InfoSetConverter::NUM_ACTION_TYPES);
            m_adv_optimizer = std::make_unique<torch::optim::Adam>(m_adv_network->parameters(), torch::optim::AdamOptions(LEARNING_RATE));

            // 2. Collect data through game traversals
            // For simplicity, running one traversal per player. This can be increased.
            traverse_cfr(m_game, p, i, 1.0, 1.0);
            m_game.reInitialize();

            // 3. Train the advantage network on the collected data
            train_advantage_network(); // This function now includes the 4000 SGD steps
        }

        // 4. Train the strategy network
        train_strategy_network();

        if (i % 10 == 0) { // Log progress periodically
            std::cout << "CFR Iteration " << i << " completed." << std::endl;
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

    int currentPlayer = game.getCurrentPlayer();
    const auto legal_actions = game.getActions();
    const int num_actions = legal_actions.size();
    std::string info_set_str = game.getInfoSet(currentPlayer);

    // Convert info set string to structured tensors
    auto [card_tensors, bet_tensor] = InfoSetConverter::convert(info_set_str);

    // Get strategy from the appropriate network
    std::shared_ptr<DeepCFRModelImpl> current_net = (currentPlayer == updatePlayer) ? m_adv_network : m_strategy_network;
    torch::Tensor strategy_output = current_net->forward(card_tensors, bet_tensor).squeeze(0);

    std::vector<float> strategy(InfoSetConverter::NUM_ACTION_TYPES, 0.0f);
    float normalizing_sum = 0.0f;
    for (const auto& action : legal_actions) {
        int action_idx = static_cast<int>(action);
        strategy[action_idx] = std::max(0.0f, strategy_output[action_idx].item<float>());
        normalizing_sum += strategy[action_idx];
    }

    if (normalizing_sum > 0.0f) {
        for (const auto& action : legal_actions) {
            int action_idx = static_cast<int>(action);
            strategy[action_idx] /= normalizing_sum;
        }
    } else {
        for (const auto& action : legal_actions) {
            strategy[static_cast<int>(action)] = 1.0f / num_actions;
        }
    }

    // External sampling: traverse based on whose turn it is
    if (currentPlayer != updatePlayer) {
        // Opponent's turn: store their strategy and sample one action
        torch::Tensor strategy_tensor = torch::from_blob(strategy.data(), {InfoSetConverter::NUM_ACTION_TYPES}).clone();
        if (m_strategy_memory.size() < MEMORY_SIZE) {
            m_strategy_memory.push_back({card_tensors, bet_tensor, strategy_tensor});
        }

        std::discrete_distribution<int> dist(strategy.begin(), strategy.end());
        int sampled_action_idx = dist(m_rng);

        GameType next_game = game;
        next_game.transition(static_cast<typename GameType::Action>(sampled_action_idx));
        return traverse_cfr(next_game, updatePlayer, current_iter, p0, p1 * strategy[sampled_action_idx]);
    }

    // Update player's turn: explore all actions and compute regrets
    std::vector<float> action_utils(InfoSetConverter::NUM_ACTION_TYPES, 0.0f);
    float node_util = 0.0f;

    for (const auto& action : legal_actions) {
        int action_idx = static_cast<int>(action);
        GameType next_game = game;
        next_game.transition(action);
        action_utils[action_idx] = traverse_cfr(next_game, updatePlayer, current_iter, p0 * strategy[action_idx], p1);
        node_util += strategy[action_idx] * action_utils[action_idx];
    }

    // Compute regrets and add to memory
    torch::Tensor iteration_regrets = torch::zeros({InfoSetConverter::NUM_ACTION_TYPES});
    for (const auto& action : legal_actions) {
        int action_idx = static_cast<int>(action);
        float regret = action_utils[action_idx] - node_util;
        iteration_regrets[action_idx] = regret;
    }

    if (m_adv_memory.size() < MEMORY_SIZE) {
        m_adv_memory.push_back({card_tensors, bet_tensor, iteration_regrets});
    }

    return node_util;
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::train_advantage_network() {
    if (m_adv_memory.empty()) return;
    std::cout << "Training advantage network for " << SGD_ITERATIONS << " steps..." << std::endl;

    m_adv_network->train();
    for (int i = 0; i < SGD_ITERATIONS; ++i) {
        // Reservoir sampling by shuffling and taking the first BATCH_SIZE elements
        std::shuffle(m_adv_memory.begin(), m_adv_memory.end(), m_rng);
        size_t current_batch_size = std::min((size_t)BATCH_SIZE, m_adv_memory.size());

        std::vector<torch::Tensor> batch_cards_h, batch_cards_f, batch_cards_t, batch_cards_r;
        std::vector<torch::Tensor> batch_bets_list;
        std::vector<torch::Tensor> batch_targets_list;

        for(size_t j = 0; j < current_batch_size; ++j) {
            batch_cards_h.push_back(m_adv_memory[j].cards[0]);
            batch_cards_f.push_back(m_adv_memory[j].cards[1]);
            batch_cards_t.push_back(m_adv_memory[j].cards[2]);
            batch_cards_r.push_back(m_adv_memory[j].cards[3]);
            batch_bets_list.push_back(m_adv_memory[j].bets);
            batch_targets_list.push_back(m_adv_memory[j].target_values);
        }

        std::vector<torch::Tensor> batch_cards = {
            torch::cat(batch_cards_h, 0), torch::cat(batch_cards_f, 0),
            torch::cat(batch_cards_t, 0), torch::cat(batch_cards_r, 0)
        };
        torch::Tensor batch_bets = torch::cat(batch_bets_list, 0);
        torch::Tensor batch_target = torch::cat(batch_targets_list, 0);

        m_adv_optimizer->zero_grad();
        torch::Tensor adv_output = m_adv_network->forward(batch_cards, batch_bets);
        torch::Tensor adv_loss = torch::mse_loss(adv_output, batch_target);
        adv_loss.backward();
        m_adv_network->clip_gradients(GRADIENT_CLIP_NORM);
        m_adv_optimizer->step();

        if (i % 1000 == 0) {
             std::cout << "  [Advantage] SGD Step " << i << ", Loss: " << adv_loss.item<float>() << std::endl;
        }
    }
    m_adv_memory.clear(); // Clear memory for the next player/iteration
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::train_strategy_network() {
    if (m_strategy_memory.size() < BATCH_SIZE) return;
    std::cout << "Training strategy network..." << std::endl;

    m_strategy_network->train();
    // A single training step for the strategy network per CFR iteration
    std::shuffle(m_strategy_memory.begin(), m_strategy_memory.end(), m_rng);

    std::vector<torch::Tensor> batch_cards_h, batch_cards_f, batch_cards_t, batch_cards_r;
    std::vector<torch::Tensor> batch_bets_list;
    std::vector<torch::Tensor> batch_targets_list;

    for(size_t j = 0; j < BATCH_SIZE; ++j) {
        batch_cards_h.push_back(m_strategy_memory[j].cards[0]);
        batch_cards_f.push_back(m_strategy_memory[j].cards[1]);
        batch_cards_t.push_back(m_strategy_memory[j].cards[2]);
        batch_cards_r.push_back(m_strategy_memory[j].cards[3]);
        batch_bets_list.push_back(m_strategy_memory[j].bets);
        batch_targets_list.push_back(m_strategy_memory[j].target_values);
    }

    std::vector<torch::Tensor> batch_cards = {
        torch::cat(batch_cards_h, 0), torch::cat(batch_cards_f, 0),
        torch::cat(batch_cards_t, 0), torch::cat(batch_cards_r, 0)
    };
    torch::Tensor batch_bets = torch::cat(batch_bets_list, 0);
    torch::Tensor batch_target = torch::cat(batch_targets_list, 0);

    m_strategy_optimizer->zero_grad();
    torch::Tensor strat_output = m_strategy_network->forward(batch_cards, batch_bets);
    torch::Tensor strat_loss = torch::mse_loss(strat_output, batch_target);
    strat_loss.backward();
    m_strategy_network->clip_gradients(GRADIENT_CLIP_NORM);
    m_strategy_optimizer->step();

    std::cout << "  [Strategy] Loss: " << strat_loss.item<float>() << std::endl;

    // Clear memory for the next batch of traversals
    m_strategy_memory.clear();
}


// Explicit template instantiation
template class DeepRegretMinimizer<Preflop::Game>;
template class DeepRegretMinimizer<Texas::Game>;