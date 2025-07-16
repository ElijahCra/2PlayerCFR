//
// Created by elijah on 7/16/25.
//

#ifndef NET_HPP
#define NET_HPP

#include <cstdint>
#include <torch/torch.h>

/// @class Net
/// @brief A simple feed-forward neural network for Deep CFR.
/// Inherits from torch::nn::Module.
struct Net : torch::nn::Module {
    /// @brief Constructor to initialize the network layers.
    /// @param input_size The size of the input feature vector (information set).
    /// @param output_size The number of actions to output regrets for.
    Net(int64_t input_size, int64_t output_size);

    /// @brief The forward pass of the network.
    /// @param x The input tensor representing a batch of information sets.
    /// @return A tensor of predicted regrets for each action.
    torch::Tensor forward(torch::Tensor x);

private:
    torch::nn::Linear fc1{nullptr}, fc2{nullptr}, fc3{nullptr};
};
#endif //NET_HPP
