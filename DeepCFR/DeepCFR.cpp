//
// Created by elijah on 7/16/25.
//

#include "DeepCFR.hpp"
#include <cassert>

template<typename GameType>
DeepRegretMinimizer<GameType>::DeepRegretMinimizer(uint32_t seed)
    : m_rng(seed),
      m_game(m_rng),
      m_adv_network(std::make_shared<Net>(InfoSetConverter::INPUT_SIZE, InfoSetConverter::NUM_ACTION_TYPES)),
      m_strategy_network(std::make_shared<Net>(InfoSetConverter::INPUT_SIZE, InfoSetConverter::NUM_ACTION_TYPES)),
      m_adv_optimizer(std::make_unique<torch::optim::Adam>(m_adv_network->parameters(), LEARNING_RATE)),
      m_strategy_optimizer(std::make_unique<torch::optim::Adam>(m_strategy_network->parameters(), LEARNING_RATE))
{
    // Ensure the utility class for hand evaluation is initialized.
    Utility::initLookup();
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::Train(uint32_t iterations) {
    for (uint32_t i = 1; i <= iterations; ++i) {
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            traverse_cfr(m_game, p, i, 1.0, 1.0);
            m_game.reInitialize();
        }

        // Periodically train the network on collected data
        if (i % 10 == 0) {
            update_networks();
        }
        std::cout << "Iteration " << i << " completed." << std::endl;
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

    // It's a decision node
    int currentPlayer = game.getCurrentPlayer();
    const auto legal_actions = game.getActions();
    const int num_actions = legal_actions.size();

    // Convert info set string to tensor
    std::string info_set_str = game.getInfoSet(currentPlayer);
    torch::Tensor info_tensor = InfoSetConverter::convert(info_set_str).unsqueeze(0);

    // Get strategy from the network
    torch::Tensor strategy_output = m_adv_network->forward(info_tensor).squeeze(0);

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
    } else { // Default to uniform random strategy
        for (const auto& action : legal_actions) {
            int action_idx = static_cast<int>(action);
            strategy[action_idx] = 1.0f / num_actions;
        }
    }

    // For external sampling, we only explore one action if not the update player
    if (currentPlayer != updatePlayer) {
        std::discrete_distribution<int> dist(strategy.begin(), strategy.end());
        int sampled_action_idx = dist(m_rng);

        GameType next_game = game;
        next_game.transition(static_cast<typename GameType::Action>(sampled_action_idx));
        return traverse_cfr(next_game, updatePlayer, current_iter, p0, p1 * strategy[sampled_action_idx]);
    }

    // If it's the update player, explore all actions
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
         m_adv_memory.push_back({info_tensor.squeeze(0), iteration_regrets, torch::zeros(1)});
    }

    // Update strategy memory
    torch::Tensor current_strategy_tensor = torch::from_blob(strategy.data(), {InfoSetConverter::NUM_ACTION_TYPES}).clone();
     if (m_strategy_memory.size() < MEMORY_SIZE) {
        m_strategy_memory.push_back({info_tensor.squeeze(0), torch::zeros(1), current_strategy_tensor});
    }

    return node_util;
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::update_networks() {
    if (m_adv_memory.size() < BATCH_SIZE) return;

    // --- Train Advantage (Regret) Network ---
    std::cout << "Training advantage network..." << std::endl;
    torch::Tensor adv_batch_infoset = torch::empty({BATCH_SIZE, InfoSetConverter::INPUT_SIZE});
    torch::Tensor adv_batch_target = torch::empty({BATCH_SIZE, InfoSetConverter::NUM_ACTION_TYPES});

    for(size_t i = 0; i < BATCH_SIZE; ++i) {
        int rand_idx = m_rng() % m_adv_memory.size();
        adv_batch_infoset[i] = m_adv_memory[rand_idx].info_set_tensor;
        adv_batch_target[i] = m_adv_memory[rand_idx].iteration_regrets;
    }

    m_adv_optimizer->zero_grad();
    torch::Tensor adv_output = m_adv_network->forward(adv_batch_infoset);
    torch::Tensor adv_loss = torch::mse_loss(adv_output, adv_batch_target);
    adv_loss.backward();
    m_adv_optimizer->step();
    std::cout << "Advantage Loss: " << adv_loss.item<float>() << std::endl;

    // --- Train Strategy Network ---
    std::cout << "Training strategy network..." << std::endl;
    torch::Tensor strat_batch_infoset = torch::empty({BATCH_SIZE, InfoSetConverter::INPUT_SIZE});
    torch::Tensor strat_batch_target = torch::empty({BATCH_SIZE, InfoSetConverter::NUM_ACTION_TYPES});

    for(size_t i = 0; i < BATCH_SIZE; ++i) {
        int rand_idx = m_rng() % m_strategy_memory.size();
        strat_batch_infoset[i] = m_strategy_memory[rand_idx].info_set_tensor;
        strat_batch_target[i] = m_strategy_memory[rand_idx].iteration_strategy;
    }

    m_strategy_optimizer->zero_grad();
    torch::Tensor strat_output = m_strategy_network->forward(strat_batch_infoset);
    torch::Tensor strat_loss = torch::mse_loss(strat_output, strat_batch_target);
    strat_loss.backward();
    m_strategy_optimizer->step();
    std::cout << "Strategy Loss: " << strat_loss.item<float>() << std::endl;

    // Clear memory for the next batch of traversals
    m_adv_memory.clear();
    m_strategy_memory.clear();
}

// Explicit template instantiation
template class DeepRegretMinimizer<Preflop::Game>;
template class DeepRegretMinimizer<Texas::Game>;