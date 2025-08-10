//
// Created by elijah on 7/16/25.
//

#include "DeepCFR.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <numeric>

#include "future_support.hpp"
#include "CoroutineAwaitables.hpp"
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
void DeepRegretMinimizer<GameType>::TrainParallel(uint32_t iterations,const size_t num_threads) {
    // You can remove TaskManager.hpp and TaskSystem.hpp from your includes.
    // Ensure you include ThreadPool.hpp.

    for (int iter = 1; iter <= K_TRAVERSALS; ++iter) {
        std::cout << "Iteration " << iter << "/" << K_TRAVERSALS << std::endl;

        for (int p = 0; p < GameType::PlayerNum; ++p) {
            auto t1 = std::chrono::high_resolution_clock::now();
            size_t startSamples = m_adv_memories[p].size();

            // 1. Move networks to GPU and setup the dispatcher
            m_advantage_networks[0]->to(m_device);
            m_advantage_networks[1]->to(m_device);
            GPUDispatcher dispatcher(m_advantage_networks, m_device);
            dispatcher.start();

            // 2. Setup the ThreadPool
            ThreadPool thread_pool(num_threads);

            // 3. Seed the initial traversals
            std::vector<std::future<float>> traversal_futures;
            for (int k = 0; k < K_TRAVERSALS; ++k) {
                // Each traversal needs its own RNG and initial game state.
                std::mt19937 local_rng(m_rng()); // Seed from the main RNG
                GameType game_for_task(local_rng);

                traversal_futures.push_back(
                    // Use the thread pool to start the coroutine
                    thread_pool.enqueue([this, game_for_task, p, iter, &thread_pool, &dispatcher, &local_rng]() mutable {
                        return this->traverse_cfr_coro(std::move(game_for_task), p, iter, thread_pool, dispatcher, local_rng).get();
                    })
                );
            }

            // 4. Wait for all root traversals for this player to complete
            for(auto& fut : traversal_futures) {
                fut.wait(); // Block on each future until it's done
            }

            auto t2 = std::chrono::high_resolution_clock::now();
            auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
            auto samplesAdded = (m_adv_memories[p].size() - startSamples);
            std::cout << "All traversals for player " << p << " completed in : " << ms_int.count() << "ms. Samples Added: " << samplesAdded << std::endl;

            // 5. Clean up dispatcher
            dispatcher.stop();

            // 6. Train the network (no change here)
            m_advantage_networks[p]->to(m_device);
            train_advantage_network(p);
        }
        std::cout << "Strat Memory Size: " << m_strategy_memory.size() << std::endl;
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
        sample.legal_action_indices = legal_indices;
        sample.advantages = instant_regrets;
        sample.weight = static_cast<float>(current_iter); // Linear weighting

        m_adv_memories[updatePlayer].add_sample(sample, m_rng);

        return nodeValue;
    } else {
        // Opponent: sample single action and store strategy
        TrainingSampleStrategy sample;
        sample.infoset = {cards_cpu,bets_cpu};  // Store original CPU tensors
        sample.legal_action_indices = legal_indices;
        sample.strategy = strategy;
        sample.weight = static_cast<float>(current_iter); // Linear weighting

        add_to_strategy_memory(m_strategy_memory, sample, MEMORY_SIZE, m_rng);

        // Sample action according to strategy
        std::discrete_distribution<> dist(strategy.begin(), strategy.end());
        int action_idx = dist(m_rng);
        GameType next_game(game);
        next_game.transition(legal_actions[action_idx]);
        return traverse_cfr(next_game, updatePlayer, current_iter, probUpdatePlayer);
    }
}

template<typename GameType>
std::future<float> DeepRegretMinimizer<GameType>::traverse_cfr_coro(GameType game, int updatePlayer, int current_iter,ThreadPool& thread_pool, GPUDispatcher& dispatcher, std::mt19937& rng)
{
    // ---- STATE 1: Terminal Node ----
    if (game.getType() == "terminal") {
        co_return game.getUtility(updatePlayer);
    }

    // ---- STATE 2: Chance Node ----
    if (game.getType() == "chance") {
        game.transition(GameType::Action::Chance);
        // "Recurse" into the coroutine. The `co_await` unwraps the future.
        co_return co_await traverse_cfr_coro(std::move(game), updatePlayer, current_iter, thread_pool, dispatcher, rng);
    }

    // ---- STATE 3: Decision Node ----
    int currentPlayer = game.getCurrentPlayer();
    auto legal_actions = game.getActions();
    std::vector<int> legal_indices;
    legal_indices.reserve(legal_actions.size());
    for (const auto& action : legal_actions) {
        legal_indices.push_back(GameType::ActionMapping::getActionIndex(action));
    }

    // Get advantages from the network. This is our first suspension point.
    // The coroutine pauses here until the GPU result is available.
    auto advantages_tensor = co_await await_gpu_inference(
        dispatcher,
        currentPlayer,
        game.getCardTensors(currentPlayer, game.getCurrentRound()),
        game.getBetTensor()
    );
    advantages_tensor = advantages_tensor.to(torch::kCPU);

    std::vector<float> legal_advantages;
    legal_advantages.reserve(legal_actions.size());
    for (int idx : legal_indices) {
        legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
    }

    // Compute strategy from advantages
    auto strategy = compute_strategy_from_advantages(legal_advantages);

    if (currentPlayer == updatePlayer) {
        // --- Traverser: Explore all actions ---

        // Launch all child traversals asynchronously.
        std::vector<std::future<float>> child_futures;
        child_futures.reserve(legal_actions.size());
        for (const auto& action : legal_actions) {
            GameType next_game(game);
            next_game.transition(action);
            child_futures.push_back(
                thread_pool.enqueue([this, next_game, updatePlayer, current_iter, &thread_pool, &dispatcher, &rng]() mutable {
                    // This lambda needs to call the coroutine and get its result.
                    // Since thread_pool.enqueue expects a callable that returns a value, not a future,
                    // we must block here with .get(). The overall process remains async.
                    return this->traverse_cfr_coro(std::move(next_game), updatePlayer, current_iter, thread_pool, dispatcher, rng).get();
                })
            );
        }

        // Wait for all child traversals to complete. This is our second suspension point.
        std::vector<float> counterfactual_values = co_await await_all(std::move(child_futures));

        // Now that children are done, compute node value and regrets.
        float nodeValue = 0.0f;
        for (size_t i = 0; i < strategy.size(); ++i) {
            nodeValue += strategy[i] * counterfactual_values[i];
        }

        std::vector<float> instant_regrets;
        instant_regrets.reserve(legal_actions.size());
        for (float cfv : counterfactual_values) {
            instant_regrets.push_back(cfv - nodeValue);
        }

        // Store in memory. (Note: This part needs to be thread-safe if multiple
        // coroutines write to memory concurrently. A simple mutex will do).
        TrainingSampleAdvantage sample;
        sample.infoset = {game.getCardTensors(currentPlayer, game.getCurrentRound()), game.getBetTensor()};
        sample.legal_action_indices = legal_indices;
        sample.advantages = instant_regrets;
        sample.weight = static_cast<float>(current_iter);

        // Assuming m_adv_memories is thread-safe or protected by a mutex
        m_adv_memories[updatePlayer].add_sample(sample, rng);

        co_return nodeValue;

    } else {
        // --- Opponent: Sample a single action ---
        TrainingSampleStrategy sample;
        sample.infoset = {game.getCardTensors(currentPlayer, game.getCurrentRound()), game.getBetTensor()};
        sample.legal_action_indices = legal_indices;
        sample.strategy = strategy;
        sample.weight = static_cast<float>(current_iter);

        // This function needs to be thread-safe
        add_to_strategy_memory(m_strategy_memory, sample, MEMORY_SIZE, rng);

        std::discrete_distribution<> dist(strategy.begin(), strategy.end());
        int action_idx = dist(rng);

        game.transition(legal_actions[action_idx]);

        // Tail-recurse into the coroutine
        co_return co_await traverse_cfr_coro(std::move(game), updatePlayer, current_iter, thread_pool, dispatcher, rng);
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




// Explicit template instantiation
template class DeepRegretMinimizer<Preflop::Game>;
template class DeepRegretMinimizer<Texas::Game>;
