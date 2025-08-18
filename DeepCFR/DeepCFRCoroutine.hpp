// DeepCFRCoroutine.hpp
#ifndef DEEPCFR_COROUTINE_HPP
#define DEEPCFR_COROUTINE_HPP

#include "CoroutineTraversal.hpp"
#include "DeepCFR.hpp"
#include <deque>

template<typename GameType>
class DeepCFRCoroutine : public DeepRegretMinimizer<GameType> {
public:
    using Base = DeepRegretMinimizer<GameType>;
    using Base::m_rng;
    using Base::m_game;
    using Base::m_device;
    using Base::m_advantage_networks;
    using Base::m_adv_memories;
    using Base::m_strategy_memory;
    using Base::compute_strategy_from_advantages;
    using Base::add_to_strategy_memory;

    explicit DeepCFRCoroutine(uint32_t seed = std::random_device()())
        : Base(seed), m_gpu_dispatcher(m_advantage_networks, m_device) {
        m_gpu_dispatcher.start();
    }

    ~DeepCFRCoroutine() {
        m_gpu_dispatcher.stop();
    }

    void TrainWithCoroutines(uint32_t iterations, size_t max_concurrent_traversals = 64) {
        for (uint32_t iter = 1; iter <= iterations; ++iter) {
            std::cout << "Iteration " << iter << "/" << iterations << std::endl;

            for (int p = 0; p < GameType::PlayerNum; ++p) {
                auto t1 = std::chrono::high_resolution_clock::now();

                // Run coroutine-based traversals
                run_coroutine_traversals(p, iter, max_concurrent_traversals);

                auto t2 = std::chrono::high_resolution_clock::now();
                auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
                std::cout << "Coroutine training for player " << p << " time: " << ms_int.count() << "ms" << std::endl;

                m_game.reInitialize();
                Base::train_advantage_network(p);
            }
        }

        Base::train_strategy_network();
    }

private:
    GPUDispatcher m_gpu_dispatcher;

    struct CoroutineContext {
        GameType game;
        int update_player;
        int current_iter;
        float prob_update_player;
        std::future<torch::Tensor> gpu_future;
        bool waiting_for_gpu = false;

        // Store intermediate state for resumption
        int current_player;
        std::vector<typename GameType::Action> legal_actions;
        std::vector<int> legal_indices;
        std::vector<torch::Tensor> cards_tensor;
        torch::Tensor bets_tensor;
    };

    void run_coroutine_traversals(int player, int iteration, size_t max_concurrent) {
        std::deque<std::unique_ptr<CoroutineContext>> active_contexts;
        int completed_traversals = 0;
        const int target_traversals = Base::K_TRAVERSALS;

        // Start initial batch of traversals
        for (size_t i = 0; i < std::min(max_concurrent, static_cast<size_t>(target_traversals)); ++i) {
            auto ctx = std::make_unique<CoroutineContext>();
            GameType next_game(m_game);
            ctx->game = next_game;
            ctx->update_player = player;
            ctx->current_iter = iteration;
            ctx->prob_update_player = 1.0f;
            active_contexts.push_back(std::move(ctx));
        }

        // Main coroutine scheduling loop
        while (!active_contexts.empty() || completed_traversals < target_traversals) {
            // Process active contexts
            for (auto it = active_contexts.begin(); it != active_contexts.end(); ) {
                auto& ctx = *it;

                if (ctx->waiting_for_gpu) {
                    // Check if GPU result is ready
                    if (ctx->gpu_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        // Resume traversal with GPU result
                        float value = resume_traversal_after_gpu(ctx);
                        completed_traversals++;

                        // Remove completed context
                        it = active_contexts.erase(it);

                        // Start new traversal if needed
                        if (active_contexts.size() + completed_traversals < target_traversals) {
                            auto new_ctx = std::make_unique<CoroutineContext>();
                            new_ctx->game = GameType(m_rng);
                            new_ctx->update_player = player;
                            new_ctx->current_iter = iteration;
                            new_ctx->prob_update_player = 1.0f;
                            active_contexts.push_back(std::move(new_ctx));
                        }
                    } else {
                        ++it;
                    }
                } else {
                    // Continue traversal until GPU needed or completion
                    float result = traverse_until_gpu_or_complete(ctx);

                    if (!ctx->waiting_for_gpu) {
                        // Traversal completed without needing GPU
                        completed_traversals++;
                        it = active_contexts.erase(it);

                        // Start new traversal if needed
                        if (active_contexts.size() + completed_traversals < target_traversals) {
                            auto new_ctx = std::make_unique<CoroutineContext>();
                            new_ctx->game = GameType(m_rng);
                            new_ctx->update_player = player;
                            new_ctx->current_iter = iteration;
                            new_ctx->prob_update_player = 1.0f;
                            active_contexts.push_back(std::move(new_ctx));
                        }
                    } else {
                        ++it;
                    }
                }
            }

            // Small sleep to prevent busy waiting
            if (!active_contexts.empty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

    float traverse_until_gpu_or_complete(std::unique_ptr<CoroutineContext>& ctx) {
        // Terminal node
        if (ctx->game.getType() == "terminal") {
            return ctx->game.getUtility(ctx->update_player);
        }

        // Chance node
        if (ctx->game.getType() == "chance") {
            ctx->game.transition(GameType::Action::Chance);
            return traverse_until_gpu_or_complete(ctx);
        }

        // Decision node - prepare for GPU computation
        ctx->current_player = ctx->game.getCurrentPlayer();
        ctx->legal_actions = ctx->game.getActions();
        ctx->legal_indices.clear();

        for (auto action : ctx->legal_actions) {
            ctx->legal_indices.push_back(GameType::ActionMapping::getActionIndex(action));
        }

        ctx->cards_tensor = ctx->game.getCardTensors(ctx->current_player, ctx->game.getCurrentRound());
        ctx->bets_tensor = ctx->game.getBetTensor();

        // Submit GPU request
        auto request = std::make_unique<ForwardRequest>();
        request->player_index = ctx->current_player;
        request->cards = {ctx->cards_tensor};
        request->bets = ctx->bets_tensor;

        ctx->gpu_future = m_gpu_dispatcher.submit(std::move(request));
        ctx->waiting_for_gpu = true;

        return 0.0f; // Placeholder - will be updated when resumed
    }

    float resume_traversal_after_gpu(std::unique_ptr<CoroutineContext>& ctx) {
        // Get GPU result
        auto advantages_tensor = ctx->gpu_future.get();

        std::vector<float> legal_advantages;
        for (int idx : ctx->legal_indices) {
            legal_advantages.push_back(advantages_tensor[0][idx].template item<float>());
        }

        auto strategy = compute_strategy_from_advantages(legal_advantages);

        float nodeValue = 0.0f;

        if (ctx->current_player == ctx->update_player) {
            // Traverser: explore all actions
            std::vector<float> counterfactualValue(ctx->legal_actions.size());

            for (size_t a = 0; a < ctx->legal_actions.size(); ++a) {
                GameType next_game(ctx->game);
                next_game.transition(ctx->legal_actions[a]);

                // Recursive traversal (simplified - in full implementation would also be coroutine-based)
                counterfactualValue[a] = traverse_recursive(next_game, ctx->update_player,
                                                           ctx->current_iter,
                                                           ctx->prob_update_player * strategy[a]);
                nodeValue += strategy[a] * counterfactualValue[a];
            }

            // Compute and store advantages
            std::vector<float> instant_regrets(ctx->legal_actions.size());
            for (size_t a = 0; a < ctx->legal_actions.size(); ++a) {
                instant_regrets[a] = counterfactualValue[a] - nodeValue;
            }

            TrainingSampleAdvantage sample;
            sample.infoset = {ctx->cards_tensor, ctx->bets_tensor};
            sample.iteration = ctx->current_iter;
            sample.legal_action_indices = ctx->legal_indices;
            sample.advantages = instant_regrets;
            sample.weight = static_cast<float>(ctx->current_iter);

            m_adv_memories[ctx->update_player].add_sample(sample, m_rng);

        } else {
            // Opponent: sample action and store strategy
            TrainingSampleStrategy sample;
            sample.infoset = {ctx->cards_tensor, ctx->bets_tensor};
            sample.iteration = ctx->current_iter;
            sample.legal_action_indices = ctx->legal_indices;
            sample.strategy = strategy;
            sample.weight = static_cast<float>(ctx->current_iter);

            add_to_strategy_memory(m_strategy_memory, sample, Base::MEMORY_SIZE);

            // Sample action
            std::discrete_distribution<> dist(strategy.begin(), strategy.end());
            int action_idx = dist(m_rng);
            ctx->game.transition(ctx->legal_actions[action_idx]);

            // Continue traversal
            nodeValue = traverse_recursive(ctx->game, ctx->update_player,
                                          ctx->current_iter, ctx->prob_update_player);
        }

        ctx->waiting_for_gpu = false;
        return nodeValue;
    }

    // Fallback recursive traversal for completing subtrees
    float traverse_recursive(const GameType& game, int updatePlayer, int current_iter, float probUpdatePlayer) {
        if (game.getType() == "terminal") {
            return game.getUtility(updatePlayer);
        }

        if (game.getType() == "chance") {
            GameType next_game(game);
            next_game.transition(GameType::Action::Chance);
            return traverse_recursive(next_game, updatePlayer, current_iter, probUpdatePlayer);
        }

        // For simplicity, using synchronous GPU call here
        // In full implementation, this would also be coroutine-based
        return Base::traverse_cfr(game, updatePlayer, current_iter, probUpdatePlayer);
    }
};

#endif // DEEPCFR_COROUTINE_HPP