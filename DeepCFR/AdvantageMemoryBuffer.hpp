//
// Created by elijah on 8/4/25.
//

#ifndef ADVANTAGEMEMORYBUFFER_HPP
#define ADVANTAGEMEMORYBUFFER_HPP

#include <vector>
#include <random>
#include "torch/torch.h"
#include "types.hpp"

// A high-performance replay buffer using a "Struct of Arrays" (SoA) layout.
// Data is stored in large, pre-allocated, contiguous tensors for extremely fast batch creation.
template<typename GameType>
class AdvantageMemoryBuffer {
public:
    // Constructor pre-allocates all tensor memory on the CPU.
    AdvantageMemoryBuffer(size_t capacity)
        : m_capacity(capacity), m_size(0), m_next_idx(0) {

        // Pre-allocate tensors for each card type (e.g., hole, flop, etc.)
        for(int i = 0; i < GameType::NUM_CARD_TYPES; ++i) {
            auto card_shape = GameType::getCardShape(i);
            m_card_tensors.push_back(torch::empty({(long)capacity, card_shape}, torch::kInt64));
        }

        // Pre-allocate tensors for bets, targets, masks, and weights.
        m_bet_tensors = torch::empty({(long)capacity, (long)GameType::NUM_BET_FEATURES});
        m_targets = torch::empty({(long)capacity, (long)GameType::MAX_ACTIONS});
        m_masks = torch::empty({(long)capacity, (long)GameType::MAX_ACTIONS});
        m_weights = torch::empty({(long)capacity, 1});
    }

    // Adds a new sample to the buffer, using reservoir sampling if full.
    void add_sample(const TrainingSampleAdvantage& sample, std::mt19937& rng);

    // Provides direct, read-only access to the underlying tensors for the DataLoader.
    const std::vector<torch::Tensor>& get_card_tensors() const { return m_card_tensors; }
    const torch::Tensor& get_bet_tensors() const { return m_bet_tensors; }
    const torch::Tensor& get_targets() const { return m_targets; }
    const torch::Tensor& get_masks() const { return m_masks; }
    const torch::Tensor& get_weights() const { return m_weights; }
    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }

private:
    size_t m_capacity;
    size_t m_size;
    size_t m_next_idx;

    // "Struct of Arrays": Each member is a large tensor containing all data for that feature.
    std::vector<torch::Tensor> m_card_tensors;
    torch::Tensor m_bet_tensors;
    torch::Tensor m_targets;
    torch::Tensor m_masks;
    torch::Tensor m_weights;
};

// Implementation of the add_sample method.
template<typename GameType>
void AdvantageMemoryBuffer<GameType>::add_sample(const TrainingSampleAdvantage& sample, std::mt19937& rng) {
    size_t index_to_write;

    if (m_size < m_capacity) {
        // Fill the buffer sequentially until it's full.
        index_to_write = m_next_idx;
        m_size++;
        m_next_idx++;
    } else {
        // Reservoir sampling: overwrite a random past sample.
        std::uniform_int_distribution<size_t> dist(0, m_size - 1);
        index_to_write = dist(rng);
    }

    // Prepare target and mask tensors for the single sample.
    torch::Tensor sample_target = torch::zeros({GameType::MAX_ACTIONS});
    torch::Tensor sample_mask = torch::zeros({GameType::MAX_ACTIONS});
    for (size_t i = 0; i < sample.legal_action_indices.size(); ++i) {
        int action_idx = sample.legal_action_indices[i];
        sample_target[action_idx] = sample.advantages[i];
        sample_mask[action_idx] = 1.0f;
    }

    // Get views (slices) of the large tensors at the target index.
    auto target_slice = m_targets.slice(0, index_to_write, index_to_write + 1);
    auto mask_slice = m_masks.slice(0, index_to_write, index_to_write + 1);
    auto weight_slice = m_weights.slice(0, index_to_write, index_to_write + 1);
    auto bet_slice = m_bet_tensors.slice(0, index_to_write, index_to_write + 1);

    // Copy the new sample's data into the pre-allocated buffer. This is very fast.
    target_slice.copy_(sample_target);
    mask_slice.copy_(sample_mask);
    weight_slice.copy_(torch::tensor({sample.weight}));
    bet_slice.copy_(sample.infoset.getBetTensor());

    for(int i = 0; i < GameType::NUM_CARD_TYPES; ++i) {
        m_card_tensors[i].slice(0, index_to_write, index_to_write + 1).copy_(sample.infoset.getCardTensors()[i]);
    }
}

#endif //ADVANTAGEMEMORYBUFFER_HPP
