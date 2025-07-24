//
// Created by elijah on 7/16/25.
//

#ifndef NET_HPP
#define NET_HPP

#include <cstdint>
#include <torch/torch.h>

#include <torch/torch.h>
#include <vector>

// Card Embedding Module
class CardEmbeddingImpl : public torch::nn::Module {
public:
    CardEmbeddingImpl(int64_t dim) {
        rank = register_module("rank", torch::nn::Embedding(13, dim));
        suit = register_module("suit", torch::nn::Embedding(4, dim));
        card = register_module("card", torch::nn::Embedding(52, dim));
    }

    torch::Tensor forward(torch::Tensor input) {
        // input shape: [B, num_cards]
        auto sizes = input.sizes();
        int64_t B = sizes[0];
        int64_t num_cards = sizes[1];

        // Flatten input
        auto x = input.view({-1});

        // Create mask for valid cards (-1 means 'no card')
        auto valid = x.ge(0).to(torch::kFloat32);

        // Clamp negative values to 0
        x = x.clamp_min(0);

        // Get embeddings
        auto card_embs = card->forward(x);
        auto rank_embs = rank->forward(torch::div(x, 4, "floor").to(torch::kLong));

        auto suit_embs = suit->forward(x % 4);

        // Sum embeddings
        auto embs = card_embs + rank_embs + suit_embs;

        // Zero out 'no card' embeddings
        embs = embs * valid.unsqueeze(1);

        // Reshape and sum across cards in the hole/board
        embs = embs.view({B, num_cards, -1});
        return embs.sum(1);  // Sum along card dimension
    }

private:
    torch::nn::Embedding rank{nullptr}, suit{nullptr}, card{nullptr};
};
TORCH_MODULE(CardEmbedding);

// Deep CFR Model
class DeepCFRModelImpl : public torch::nn::Module {
public:
    DeepCFRModelImpl() = default;
    
    DeepCFRModelImpl(int64_t n_card_types, int64_t n_bets, int64_t n_actions, int64_t dim = 256) {
        // Initialize card embeddings
        card_embeddings = torch::nn::ModuleList();
        for (int64_t i = 0; i < n_card_types; ++i) {
            card_embeddings->push_back(CardEmbedding(dim));
        }
        register_module("card_embeddings", card_embeddings);

        // Card branch layers
        card1 = register_module("card1", torch::nn::Linear(dim * n_card_types, dim));
        card2 = register_module("card2", torch::nn::Linear(dim, dim));
        card3 = register_module("card3", torch::nn::Linear(dim, dim));

        // Bet branch layers
        bet1 = register_module("bet1", torch::nn::Linear(n_bets * 2, dim));
        bet2 = register_module("bet2", torch::nn::Linear(dim, dim));

        // Combined trunk layers
        comb1 = register_module("comb1", torch::nn::Linear(2 * dim, dim));
        comb2 = register_module("comb2", torch::nn::Linear(dim, dim));
        comb3 = register_module("comb3", torch::nn::Linear(dim, dim));

        // Action head
        action_head = register_module("action_head", torch::nn::Linear(dim, n_actions));
    }

    torch::Tensor forward(const std::vector<torch::Tensor>& cards, torch::Tensor bets) {
        //std::cout << "card list size: "<<cards.size() << " cards: " << cards[0].sizes() << std::endl;
        //std::cout << "bets: " <<bets.sizes() << std::endl;
        // 1. Card branch
        std::vector<torch::Tensor> card_embs_list;

        // Embed hole, flop, and optionally turn and river
        for (size_t i = 0; i < cards.size(); ++i) {
            auto emb = card_embeddings[i]->as<CardEmbedding>()->forward(cards[i]);
            card_embs_list.push_back(emb);
        }

        // Concatenate all card embeddings
        auto card_embs = torch::cat(card_embs_list, 1);

        // Card branch forward pass
auto x = torch::relu(card1->forward(card_embs));
        x = torch::relu(card2->forward(x));
        x = torch::relu(card3->forward(x));

        // 2. Bet branch
        auto bet_size = bets.clamp(0, 1e6);
        auto bet_occurred = bets.ge(0);

        // Concatenate bet features
        auto bet_feats = torch::cat({bet_size, bet_occurred}, 1).to(torch::kFloat32);

        // Bet branch forward pass with residual connection
        auto y = torch::relu(bet1->forward(bet_feats));
        y = torch::relu(bet2->forward(y) + y);

        // 3. Combined trunk
        auto z = torch::cat({x, y}, 1);
        z = torch::relu(comb1->forward(z));
        z = torch::relu(comb2->forward(z) + z);  // Residual connection
        z = torch::relu(comb3->forward(z) + z);  // Residual connection


        //std::cout << z.sizes() << std::endl;
        // Normalize (z - mean) / std
        z = normalize(z);



        // Return action logits
        return action_head->forward(z);
    }

    /// @brief Clips the gradients of the model's parameters.
    void clip_gradients(double clip_value) {
        auto params = this->parameters();
        if (!params.empty()) {
            torch::nn::utils::clip_grad_norm_(params, clip_value);
        }
    }

private:
    torch::nn::ModuleList card_embeddings;
    torch::nn::Linear card1{nullptr}, card2{nullptr}, card3{nullptr};
    torch::nn::Linear bet1{nullptr}, bet2{nullptr};
    torch::nn::Linear comb1{nullptr}, comb2{nullptr}, comb3{nullptr};
    torch::nn::Linear action_head{nullptr};

    // Normalization function
    static torch::Tensor normalize(torch::Tensor z) {
        if (z.dim() == 1) {
            // Single sample case: normalize across features
            auto mean = z.mean();
            auto std = z.std();
            return (z - mean) / (std + 1e-8);
        } else {
            // Batched case: normalize each sample independently across features (dim=1)
            auto mean = z.mean(1, true);  // [batch_size, 1]
            auto std = z.std(1, true);    // [batch_size, 1]
            return (z - mean) / (std + 1e-8);
        }
    }
};
TORCH_MODULE(DeepCFRModel);

#endif //NET_HPP