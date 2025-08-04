//
// Created by elijah on 8/4/25.
//

#ifndef TYPES_HPP
#define TYPES_HPP

#include <vector>
#include "torch/torch.h"

// A struct to hold the components of an information set.
// Tensors are kept on the CPU until batched for training.
struct InfoSet
{
    std::vector<torch::Tensor> cardTensors;
    torch::Tensor betTensor;

    [[nodiscard]] std::vector<torch::Tensor> getCardTensors() const { return cardTensors; }
    [[nodiscard]] torch::Tensor getBetTensor() const { return betTensor; }
};

// A struct for storing samples in the advantage replay buffer.
struct TrainingSampleAdvantage {
    InfoSet infoset;
    int iteration;
    std::vector<float> advantages;  // r_tilde(I, a) for each legal action
    std::vector<int> legal_action_indices;
    float weight;                   // Weight for Linear CFR (typically the iteration number)
};

// A struct for storing samples in the strategy replay buffer.
struct TrainingSampleStrategy
{
    InfoSet infoset;
    int iteration;
    std::vector<float> strategy;
    std::vector<int> legal_action_indices;
    float weight;
};

#endif //TYPES_HPP
