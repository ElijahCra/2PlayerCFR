//
// Created by elijah on 7/16/25.
//

#include "DeepCFR.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <numeric>

template<typename GameType>
DeepRegretMinimizer<GameType>::DeepRegretMinimizer(uint32_t seed)
    : m_rng(seed),
      m_game(m_rng),
      m_strategy_network(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES, GameType::MAX_ACTIONS),
      m_strategy_optimizer({m_strategy_network->parameters()}, torch::optim::AdamOptions(LEARNING_RATE))
{
    Utility::initLookup();

    // Initialize advantage networks and optimizers for each player
    for (int i = 0; i < 2; ++i)
    {
        m_advantage_networks[i] = DeepCFRModel(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES,
                                               GameType::MAX_ACTIONS);
        m_advantage_optimizers.emplace_back(m_advantage_networks[i]->parameters(), LEARNING_RATE);

        // Reserve memory for replay buffers
        m_adv_memories[i].reserve(MEMORY_SIZE);
    }
    m_strategy_memory.reserve(MEMORY_SIZE);

    // Set networks to training mode
    m_strategy_network->train();
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::Train(uint32_t iterations) {
    for (uint32_t iter = 1; iter <= iterations; ++iter) {
        std::cout << "Iteration " << iter << "/" << iterations << std::endl;

        // Alternate between players (external sampling)
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            // Perform K traversals for this player
            for (int k = 0; k < K_TRAVERSALS; ++k) {
                // Traverse game tree with external sampling
                traverse_cfr(m_game, p, iter, 1.0f, 1.0f);
            }
            m_game.reInitialize();

            // Train advantage network from scratch for this player
            train_advantage_network(p);
        }

        // Periodically train strategy network
        if (iter % 10 == 0) {
            train_strategy_network();
        }
    }

    // Final training of strategy network
    train_strategy_network();
}

template<typename GameType>
float DeepRegretMinimizer<GameType>::traverse_cfr(const GameType& game, int updatePlayer, int current_iter, float p0, float p1) {
    // Terminal node
    if (game.getType() == "terminal") {
        return game.getUtility(updatePlayer);
    }

    // Chance node
    if (game.getType() == "chance") {
        GameType next_game(game);
        next_game.transition(GameType::Action::None);
        return traverse_cfr(next_game, updatePlayer, current_iter, p0, p1);
    }

    int currentPlayer = game.getCurrentPlayer();
    //auto infoset = game.getInfoSet(currentPlayer);
    auto legal_actions = game.getActions();

    // Get current strategy from advantage network
    torch::NoGradGuard no_grad;
    auto cards = game.getCardTensors(game.getCurrentPlayer(),game.getCurrentRound());
    auto bets = game.getBetTensor();
    std::cout << "bets sizes: "<<bets.sizes() << std::endl;

    auto advantages_tensor = m_advantage_networks[currentPlayer]->forward(cards, bets);
    std::vector<float> advantages(advantages_tensor.template data_ptr<float>(),
                                 advantages_tensor.template data_ptr<float>() + advantages_tensor.numel());

    // Compute strategy using regret matching
    auto strategy = compute_strategy_from_advantages(advantages);

    if (currentPlayer == updatePlayer) {
        // Traverser: explore all actions
        std::vector<float> action_values(legal_actions.size());

        for (size_t a = 0; a < legal_actions.size(); ++a) {
            GameType next_game = game;
            next_game.transition(legal_actions[a]);

            float new_p0 = (currentPlayer == 0) ? p0 * strategy[a] : p0;
            float new_p1 = (currentPlayer == 1) ? p1 * strategy[a] : p1;

            action_values[a] = traverse_cfr(next_game, updatePlayer, current_iter, new_p0, new_p1);
        }

        // Compute counterfactual value
        float cfr_value = 0.0f;
        for (size_t a = 0; a < legal_actions.size(); ++a) {
            cfr_value += strategy[a] * action_values[a];
        }

        // Compute advantages (instantaneous regrets)
        std::vector<float> instant_regrets(legal_actions.size());
        for (size_t a = 0; a < legal_actions.size(); ++a) {
            instant_regrets[a] = action_values[a] - cfr_value;
        }

        // Store in memory with Linear CFR weighting
        TrainingSampleAdvantage sample;
        sample.iteration = current_iter;
        sample.advantages = instant_regrets;
        sample.weight = static_cast<float>(current_iter); // Linear weighting

        add_to_memory(m_adv_memories[updatePlayer], sample, MEMORY_SIZE);

        return cfr_value;
    } else {
        // Opponent: sample single action and store strategy
        TrainingSampleStrategy sample;
        sample.iteration = current_iter;
        sample.strategy = strategy;
        sample.weight = static_cast<float>(current_iter); // Linear weighting

        add_to_memory(m_strategy_memory, sample, MEMORY_SIZE);

        // Sample action according to strategy
        std::discrete_distribution<> dist(strategy.begin(), strategy.end());
        int action_idx = dist(m_rng);

        GameType next_game = game;
        next_game.transition(legal_actions[action_idx]);

        float new_p0 = (currentPlayer == 0) ? p0 * strategy[action_idx] : p0;
        float new_p1 = (currentPlayer == 1) ? p1 * strategy[action_idx] : p1;

        return traverse_cfr(next_game, updatePlayer, current_iter, new_p0, new_p1);
    }
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::train_advantage_network(int player) {
    if (m_adv_memories[player].empty()) return;

    // Reinitialize network (train from scratch)
    m_advantage_networks[player] = DeepCFRModel(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES, GameType::MAX_ACTIONS);
    m_advantage_optimizers[player] = torch::optim::Adam(m_advantage_networks[player]->parameters(), LEARNING_RATE);
    m_advantage_networks[player]->train();

    // Training loop
    for (int iter = 0; iter < SGD_ITERATIONS; ++iter) {
        // Sample batch
        std::vector<int> indices(m_adv_memories[player].size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), m_rng);

        int batch_size = std::min(BATCH_SIZE, static_cast<const size_t>(m_adv_memories[player].size()));

        torch::Tensor total_loss = torch::zeros({1});

        for (int b = 0; b < batch_size; ++b) {
            const auto& sample = m_adv_memories[player][indices[b]];

            // Get network prediction
            auto cards = sample.infoset.getCardTensors();
            auto bets = sample.infoset.getBetTensor();
            auto predicted = m_advantage_networks[player]->forward(cards, bets);

            // Compute weighted MSE loss
            auto target = torch::from_blob(const_cast<float*>(sample.advantages.data()),
                                          {static_cast<long>(sample.advantages.size())});

            // Normalize weight as per Linear CFR
            float normalized_weight = sample.weight / m_adv_memories[player].back().iteration;
            auto loss = normalized_weight * torch::mse_loss(predicted.squeeze(), target);

            total_loss += loss;
        }

        // Backprop
        m_advantage_optimizers[player].zero_grad();
        total_loss.backward();

        // Gradient clipping
        m_advantage_networks[player]->clip_gradients(GRADIENT_CLIP_NORM);

        m_advantage_optimizers[player].step();

        if (iter % 1000 == 0) {
            std::cout << "Player " << player << " advantage network training iter " << iter
                     << ", loss: " << total_loss.item<float>() / batch_size << std::endl;
        }
    }
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::train_strategy_network() {
    if (m_strategy_memory.empty()) return;

    m_strategy_network->train();

    // Training loop
    for (int iter = 0; iter < SGD_ITERATIONS; ++iter) {
        // Sample batch
        std::vector<int> indices(m_strategy_memory.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), m_rng);

        int batch_size = std::min(BATCH_SIZE, static_cast<size_t>(m_strategy_memory.size()));

        torch::Tensor total_loss = torch::zeros({1});

        for (int b = 0; b < batch_size; ++b) {
            const auto& sample = m_strategy_memory[indices[b]];

            // Get network prediction (logits)
            auto cards = sample.infoset.getCardTensors();
            auto bets = sample.infoset.getBetTensor();
            auto logits = m_strategy_network->forward(cards, bets);

            // Convert to probabilities
            auto predicted_probs = torch::softmax(logits, -1);

            // Compute weighted cross-entropy loss
            auto target = torch::from_blob(const_cast<float*>(sample.strategy.data()),
                                          {static_cast<long>(sample.strategy.size())});

            // Normalize weight
            float normalized_weight = sample.weight / m_strategy_memory.back().iteration;

            // Cross entropy: -sum(target * log(predicted))
            auto loss = normalized_weight * (-torch::sum(target * torch::log(predicted_probs.squeeze() + 1e-8)));

            total_loss += loss;
        }

        // Backprop
        m_strategy_optimizer.zero_grad();
        total_loss.backward();

        // Gradient clipping
        m_strategy_network->clip_gradients(GRADIENT_CLIP_NORM);

        m_strategy_optimizer.step();

        if (iter % 1000 == 0) {
            std::cout << "Strategy network training iter " << iter
                     << ", loss: " << total_loss.item<float>() / batch_size << std::endl;
        }
    }
}

template<typename GameType>
std::vector<float> DeepRegretMinimizer<GameType>::compute_strategy_from_advantages(const std::vector<float>& advantages) {
    std::vector<float> strategy(advantages.size());

    // Compute positive regrets
    std::vector<float> positive_regrets(advantages.size());
    float sum_positive = 0.0f;

    for (size_t i = 0; i < advantages.size(); ++i) {
        positive_regrets[i] = std::max(0.0f, advantages[i]);
        sum_positive += positive_regrets[i];
    }

    if (sum_positive > 0) {
        // Regret matching
        for (size_t i = 0; i < advantages.size(); ++i) {
            strategy[i] = positive_regrets[i] / sum_positive;
        }
    } else {
        // Uniform strategy when all regrets are negative
        float uniform_prob = 1.0f / advantages.size();
        std::fill(strategy.begin(), strategy.end(), uniform_prob);
    }

    return strategy;
}

template<typename GameType>
template<typename T>
void DeepRegretMinimizer<GameType>::add_to_memory(std::vector<T>& memory, const T& sample, size_t max_size) {
    if (memory.size() < max_size) {
        memory.push_back(sample);
    } else {
        // Reservoir sampling
        std::uniform_int_distribution<size_t> dist(0, memory.size());
        size_t idx = dist(m_rng);
        if (idx < max_size) {
            memory[idx] = sample;
        }
    }
}

// Explicit template instantiation
template class DeepRegretMinimizer<Preflop::Game>;
template class DeepRegretMinimizer<Texas::Game>;
