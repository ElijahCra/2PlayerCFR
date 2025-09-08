//
// Created by elijah on 9/8/25.
//

// TrainingSampleSerializer.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <torch/torch.h>
#include "types.hpp" // Assuming this contains TrainingSampleAdvantage and InfoSet

class TrainingSampleSerializer {
public:
    static std::string serialize(const TrainingSampleAdvantage& sample);
    static std::unique_ptr<TrainingSampleAdvantage> deserialize(const std::string& data);

private:
    // Helper methods for tensor serialization
    static void serializeTensor(std::vector<uint8_t>& buffer, const torch::Tensor& tensor);
    static torch::Tensor deserializeTensor(const uint8_t*& ptr, const uint8_t* end);

    // Helper methods for primitive serialization
    template<typename T>
    static void writeValue(std::vector<uint8_t>& buffer, const T& value);

    template<typename T>
    static T readValue(const uint8_t*& ptr);

    static void writeVector(std::vector<uint8_t>& buffer, const std::vector<float>& vec);
    static void writeVector(std::vector<uint8_t>& buffer, const std::vector<int>& vec);

    static std::vector<float> readFloatVector(const uint8_t*& ptr);
    static std::vector<int> readIntVector(const uint8_t*& ptr);
};
