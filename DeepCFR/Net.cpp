//
// Created by elijah on 7/16/25.
//
#include "Net.hpp"

Net::Net(int64_t input_size, int64_t output_size)
    : fc1(input_size, 512),
      fc2(512, 512),
      fc3(512, output_size)
{
    // Register modules to ensure they are properly tracked
    register_module("fc1", fc1);
    register_module("fc2", fc2);
    register_module("fc3", fc3);
}

torch::Tensor Net::forward(torch::Tensor x) {
    // Apply ReLU activation function after each of the first two layers
    x = torch::relu(fc1(x));
    x = torch::relu(fc2(x));
    // The final layer provides the raw regret/strategy values
    x = fc3(x);
    return x;
}
