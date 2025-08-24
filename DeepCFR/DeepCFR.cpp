//
// Created by elijah on 7/16/25.
//

#include "DeepCFR.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <numeric>

#include "DataLoader.hpp"

template<typename GameType>
DeepRegretMinimizer<GameType>::DeepRegretMinimizer(uint32_t seed)
    : m_rng(seed),
      m_game(m_rng),
      m_device(torch::cuda::is_available() ? torch::kCUDA : torch::mps::is_available() ? torch::kMPS : torch::kCPU),
      m_strategy_network(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES, GameType::MAX_ACTIONS),
      m_strategy_optimizer({m_strategy_network->parameters()}, torch::optim::AdamOptions(LEARNING_RATE)),
      m_adv_memories{AdvantageMemoryBuffer<GameType>(MEMORY_SIZE), AdvantageMemoryBuffer<GameType>(MEMORY_SIZE)}
{
    std::cout << "Using device: " << (m_device.is_cuda() ? "CUDA" : torch::mps::is_available() ? "METAL" : "CPU") << std::endl;
    
    Utility::initLookup();

    // Move strategy network to device
    m_strategy_network->to(m_device);

    // Initialize advantage networks and optimizers for each player
    for (int i = 0; i < 2; ++i)
    {
        m_advantage_networks[i] = DeepCFRModel(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES,
                                               GameType::MAX_ACTIONS);
        m_advantage_optimizers.emplace_back(m_advantage_networks[i]->parameters(), LEARNING_RATE);

        // Reserve memory for replay buffers
        //m_adv_memories[i].reserve(MEMORY_SIZE);
    }
    m_strategy_memory.reserve(MEMORY_SIZE);
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::Train(uint32_t iterations) {
    for (uint32_t iter = 1; iter <= iterations; ++iter) {
        std::cout << "Iteration " << iter << "/" << iterations << std::endl;

        // Alternate between players (external sampling)
        for (int p = 0; p < GameType::PlayerNum; ++p) {
            m_advantage_networks[0]->to(torch::kCPU);
            m_advantage_networks[1]->to(torch::kCPU);
            auto t1 = std::chrono::high_resolution_clock::now();

            // Perform K traversals for this player
            for (int k = 0; k < K_TRAVERSALS; ++k) {
                // Traverse game tree with external sampling
                traverse_cfr(m_game, p, iter, 1.0f);
            }
            m_game.reInitialize();

            auto t2 = std::chrono::high_resolution_clock::now();
            auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
            std::cout << "Training for player: "<<p<<" time: " << ms_int << std::endl;
            // Train advantage network from scratch for this player
            m_advantage_networks[p]->to(m_device);

            train_advantage_network(p);
        }
    }

    // Final training of strategy network
    train_strategy_network();
}

template<typename GameType>
float DeepRegretMinimizer<GameType>::traverse_cfr(const GameType& game, int updatePlayer, int current_iter, float probUpdatePlayer) {
    // Terminal node
    if (game.getType() == "terminal") {
        return game.getUtility(updatePlayer);
    }

    // Chance node
    if (game.getType() == "chance") {
        GameType next_game(game);
        next_game.transition(GameType::Action::Chance);
        return traverse_cfr(next_game, updatePlayer, current_iter, probUpdatePlayer);
    }

    int currentPlayer = game.getCurrentPlayer();
    //auto infoset = game.getInfoSet(currentPlayer);
    auto legal_actions = game.getActions();
    std::vector<int> legal_indices;
    for (auto action : legal_actions) {
        legal_indices.push_back(GameType::ActionMapping::getActionIndex(action));
    }
    // Get current strategy from advantage network
    //torch::NoGradGuard no_grad;
    auto cards_cpu = game.getCardTensors(game.getCurrentPlayer(), game.getCurrentRound());
    auto bets_cpu = game.getBetTensor();
    
    torch::NoGradGuard no_grad;
    auto advantages_tensor = m_advantage_networks[currentPlayer]->forward(cards_cpu, bets_cpu);

    // std::vector<float> advantages(advantages_tensor.template data_ptr<float>(),
    //                              advantages_tensor.template data_ptr<float>() + advantages_tensor.numel());
    std::vector<float> legal_advantages;
    for (int idx : legal_indices) {
        legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
    }

    // Compute strategy using regret matching
    auto strategy = compute_strategy_from_advantages(legal_advantages);

    float nodeValue = 0.0f;
    if (currentPlayer == updatePlayer) {
        // Traverser: explore all actions
        std::vector<float> counterfactualValue(legal_actions.size());

        for (size_t a = 0; a < legal_actions.size(); ++a) {
            GameType next_game(game);
            next_game.transition(legal_actions[a]);
            counterfactualValue[a] = traverse_cfr(next_game, updatePlayer, current_iter, probUpdatePlayer * strategy[a]);
            nodeValue += strategy[a] * counterfactualValue[a];
        }

        // Compute advantages (instantaneous regrets)
        std::vector<float> instant_regrets(legal_actions.size());
        for (size_t a = 0; a < legal_actions.size(); ++a) {
            instant_regrets[a] = counterfactualValue[a] - nodeValue;
        }

        // Store in memory with Linear CFR weighting (keep tensors on CPU for memory efficiency)
        TrainingSampleAdvantage sample;
        sample.infoset = {cards_cpu, bets_cpu};
        sample.iteration = current_iter;
        sample.legal_action_indices = legal_indices;
        sample.advantages = instant_regrets;
        sample.weight = static_cast<float>(current_iter); // Linear weighting

        m_adv_memories[updatePlayer].add_sample(sample, m_rng);

        return nodeValue;
    } else {
        // Opponent: sample single action and store strategy
        TrainingSampleStrategy sample;
        sample.infoset = {cards_cpu,bets_cpu};  // Store original CPU tensors
        sample.iteration = current_iter;
        sample.legal_action_indices = legal_indices;
        sample.strategy = strategy;
        sample.weight = static_cast<float>(current_iter); // Linear weighting

        add_to_strategy_memory(m_strategy_memory, sample, MEMORY_SIZE);

        // Sample action according to strategy
        std::discrete_distribution<> dist(strategy.begin(), strategy.end());
        int action_idx = dist(m_rng);
        GameType next_game(game);
        next_game.transition(legal_actions[action_idx]);
        return traverse_cfr(next_game, updatePlayer, current_iter, probUpdatePlayer);
    }
}

template<typename GameType>
void DeepRegretMinimizer<GameType>::train_advantage_network(int player) {
    // Don't train if the memory buffer is smaller than one batch.
    if (m_adv_memories[player].size() < BATCH_SIZE) {
        std::cout << "Player " << player << " advantage network training skipped: not enough samples." << std::endl;
        return;
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    // Reinitialize network and optimizer to train from scratch.
    m_advantage_networks[player] = DeepCFRModel(GameType::NUM_CARD_TYPES, GameType::NUM_BET_FEATURES, GameType::MAX_ACTIONS);
    m_advantage_networks[player]->to(m_device);
    m_advantage_optimizers[player] = torch::optim::Adam(m_advantage_networks[player]->parameters(), LEARNING_RATE);
    m_advantage_networks[player]->train();

    // Determine how many samples and batches to train on.
    size_t total_samples_to_train = std::min(BATCH_SIZE * SGD_ITERATIONS, m_adv_memories[player].size());
    int total_batches = total_samples_to_train / BATCH_SIZE;

    // Instantiate and start the asynchronous data loader.
    DataLoader<GameType> data_loader(m_adv_memories[player], BATCH_SIZE, total_batches, m_device, m_rng);
    data_loader.start();

    float total_loss = 0.0f;

    // The main training loop.
    for (int i = 0; i < total_batches; ++i) {
        // Get a pre-fetched, pre-processed batch from the worker thread.
        // This call will block until a batch is ready.
        auto batch = data_loader.get_batch();
        if (!batch.is_valid) {
            break; // End of the data stream from the loader.
        }

        // --- Forward and Backward Pass ---
        // Data is already on the correct device. The CPU-intensive work is gone!
        auto predictions = m_advantage_networks[player]->forward(batch.cards, batch.bets);
        auto squared_error = (predictions - batch.targets).pow(2);
        auto weighted_error = squared_error * batch.masks * batch.weights;
        auto loss = weighted_error.sum() / ((batch.masks * batch.weights).sum() + 1e-9);

        m_advantage_optimizers[player].zero_grad();
        loss.backward();
        m_advantage_networks[player]->clip_gradients(GRADIENT_CLIP_NORM);
        m_advantage_optimizers[player].step();
        total_loss += loss.template item<float>();
    }

    // The DataLoader's destructor will automatically call stop() and join the thread.

    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    long long ms_count = ms_int.count();
    if (ms_count == 0) ms_count = 1; // Avoid division by zero for metrics.

    std::cout << "Player " << player << " advantage network training for " << total_samples_to_train << " samples, total loss: " << total_loss
              << " loss/sample " << total_loss / total_samples_to_train
              << " samples/ms: " << total_samples_to_train / ms_count
              << " iter runtime " << ms_int << std::endl;
}




template<typename GameType>
void DeepRegretMinimizer<GameType>::train_strategy_network() {
    if (m_strategy_memory.empty()) return;

    m_strategy_network->train();

    //Prep batches
    int total_samples = std::min(BATCH_SIZE * SGD_ITERATIONS, m_strategy_memory.size());
    std::vector<std::vector<torch::Tensor>> all_cards_cpu(total_samples);
    std::vector<torch::Tensor> all_bets_cpu(total_samples);
    std::vector<std::vector<float>> all_targets_cpu(total_samples);
    std::vector<std::vector<float>> all_masks_cpu(total_samples);
    std::vector<float> all_weights_cpu(total_samples);

    std::vector<int> all_indices(total_samples);
    std::iota(all_indices.begin(), all_indices.end(), 0);
    std::ranges::shuffle(all_indices,m_rng);

    for (int i = 0; i < total_samples; ++i) {
        const auto& sample = m_strategy_memory[all_indices[i]];
        all_cards_cpu[i] = sample.infoset.getCardTensors();
        all_bets_cpu[i] = sample.infoset.getBetTensor();
        all_weights_cpu[i] = sample.weight;

        // Prepare targets and masks
        std::vector<float> targets(GameType::MAX_ACTIONS, 0.0f);
        std::vector<float> masks(GameType::MAX_ACTIONS, 0.0f);

        for (size_t j = 0; j < sample.legal_action_indices.size(); ++j) {
            int action_idx = sample.legal_action_indices[j];
            targets[action_idx] = sample.strategy[j];
            masks[action_idx] = 1.0f;
        }

        all_targets_cpu[i] = targets;
        all_masks_cpu[i] = masks;
    }


    // Bulk GPU transfer using torch::stack
    std::vector<torch::Tensor> all_cards_batched(GameType::NUM_CARD_TYPES);
    for (int card_type = 0; card_type < GameType::NUM_CARD_TYPES; ++card_type) {
        std::vector<torch::Tensor> cards_for_type;
        cards_for_type.reserve(total_samples);
        for (int i = 0; i < total_samples; ++i) {
            cards_for_type.push_back(all_cards_cpu[i][card_type]);
        }
        all_cards_batched[card_type] = torch::stack(cards_for_type).squeeze(1).to(m_device);
    }

    auto all_bets_batched = torch::stack(all_bets_cpu).squeeze(1).to(m_device);
    auto all_weights_batched = torch::from_blob(all_weights_cpu.data(), {total_samples, 1}).clone().to(m_device);
    // Convert targets and masks to tensors
    torch::Tensor all_targets_batched = torch::zeros({total_samples, GameType::MAX_ACTIONS}, m_device);
    torch::Tensor all_masks_batched = torch::zeros({total_samples, GameType::MAX_ACTIONS}, m_device);

    for (int i = 0; i < total_samples; ++i) {
        for (int j = 0; j < GameType::MAX_ACTIONS; ++j) {
            all_targets_batched[i][j] = all_targets_cpu[i][j];
            all_masks_batched[i][j] = all_masks_cpu[i][j];
        }
    }
    // Training loop
    for (int iter = 0; iter < SGD_ITERATIONS; ++iter) {
        auto t1 = std::chrono::high_resolution_clock::now();
        int batch_start = iter * BATCH_SIZE;
        int batch_end = std::min(batch_start + BATCH_SIZE, static_cast<size_t>(total_samples));

        // Skip empty batches
        if (batch_start >= total_samples || batch_end <= batch_start) {
            continue;
        }

        // Create batch tensors
        std::vector<torch::Tensor> batch_cards;
        for (int i = 0; i < GameType::NUM_CARD_TYPES; ++i) {
            batch_cards.push_back(all_cards_batched[i].slice(0, batch_start, batch_end));
        }
        auto batch_bets = all_bets_batched.slice(0, batch_start, batch_end);
        auto batch_targets = all_targets_batched.slice(0, batch_start, batch_end);
        auto batch_masks = all_masks_batched.slice(0, batch_start, batch_end);
        auto batch_weights = all_weights_batched.slice(0, batch_start, batch_end);

        // Get raw logits from the network
        auto logits = m_strategy_network->forward(batch_cards, batch_bets);

        // Mask illegal actions before calculating loss
        auto illegal_action_mask = (batch_masks == 0);
        logits = logits.masked_fill_(illegal_action_mask, -1e9);
        // Calculate cross-entropy loss for soft targets
        auto log_probabilities = torch::log_softmax(logits, -1);
        auto loss_per_element = -(batch_targets * log_probabilities);
        auto weighted_loss_per_element = loss_per_element * batch_masks * batch_weights;
        auto masked_loss = weighted_loss_per_element.sum() / ((batch_masks * batch_weights).sum() + 1e-9);

        // Backprop
        m_strategy_optimizer.zero_grad();
        masked_loss.backward();
        m_strategy_network->clip_gradients(GRADIENT_CLIP_NORM);
        m_strategy_optimizer.step();

        auto t2 = std::chrono::high_resolution_clock::now();
        auto ms_int = duration_cast<std::chrono::milliseconds>(t2 - t1);

        std::cout << "Strategy network training iter " << iter << ", loss: " << masked_loss.template item<float>() << " time: "<< ms_int<<std::endl;
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
void DeepRegretMinimizer<GameType>::add_to_strategy_memory(std::vector<T>& memory, const T& sample, size_t max_size) {
    if (memory.size() < max_size) {
        memory.push_back(sample);
    } else {
        // Reservoir sampling
        std::cout << "reservoir" << std::endl;
        std::uniform_int_distribution<size_t> dist(0, memory.size());
        size_t idx = dist(m_rng);
        if (idx < max_size) {
            memory[idx] = sample;
        }
    }
}


// This would be a new member function in your DeepRegretMinimizer class
template<typename GameType>
CoroutineTask<float> DeepRegretMinimizer<GameType>::traverse_cfr_coro(
    const GameType& game, int updatePlayer, int current_iter, float probUpdatePlayer, GPUDispatcher& dispatcher)
{
    if (game.getType() == "terminal") {
        co_return game.getUtility(updatePlayer);
    }

    if (game.getType() == "chance") {
        GameType next_game(game);
        next_game.transition(GameType::Action::Chance);
        co_return co_await traverse_cfr_coro(next_game, updatePlayer, current_iter, probUpdatePlayer, dispatcher);
    }

    int currentPlayer = game.getCurrentPlayer();
    auto legal_actions = game.getActions();
    std::vector<int> legal_indices;
    for (auto action : legal_actions) {
        legal_indices.push_back(GameType::ActionMapping::getActionIndex(action));
    }

    // --- THIS IS THE KEY CHANGE ---
    auto cards_cpu = game.getCardTensors(game.getCurrentPlayer(), game.getCurrentRound());
    auto bets_cpu = game.getBetTensor();

    // Asynchronously await the GPU result without blocking the thread
    co_await GpuAwaitable(dispatcher, cards_cpu, bets_cpu, currentPlayer);

    // The result is now in our promise, put there by the dispatcher
    auto advantages_tensor = std::coroutine_handle<typename CoroutineTask<float>::promise_type>::from_promise(
        co_await std::experimental::this_coroutine::promise
    ).promise().m_gpu_result;

    std::vector<float> legal_advantages;
    for (int idx : legal_indices) {
        legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
    }

    auto strategy = compute_strategy_from_advantages(legal_advantages);
    float nodeValue = 0.0f;

    if (currentPlayer == updatePlayer) {
        std::vector<float> counterfactualValue(legal_actions.size());
        for (size_t a = 0; a < legal_actions.size(); ++a) {
            GameType next_game(game);
            next_game.transition(legal_actions[a]);
            // Await the recursive call
            counterfactualValue[a] = co_await traverse_cfr_coro(next_game, updatePlayer, current_iter, probUpdatePlayer * strategy[a], dispatcher);
            nodeValue += strategy[a] * counterfactualValue[a];
        }
        // ... (rest of logic for storing regrets is the same) ...
        co_return nodeValue;
    } else {
        // ... (logic for opponent is mostly the same) ...
        std::discrete_distribution<> dist(strategy.begin(), strategy.end());
        int action_idx = dist(m_rng);
        GameType next_game(game);
        next_game.transition(legal_actions[action_idx]);
        // Await the recursive call
        co_return co_await traverse_cfr_coro(next_game, updatePlayer, current_iter, probUpdatePlayer, dispatcher);
    }
}

// Explicit template instantiation
template class DeepRegretMinimizer<Preflop::Game>;
template class DeepRegretMinimizer<Texas::Game>;
