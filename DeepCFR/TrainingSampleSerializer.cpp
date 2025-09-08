//
// Created by elijah on 9/8/25.
//
#include "TrainingSampleSerializer.hpp"
#include <cstring>
#include <stdexcept>

template<typename T>
void TrainingSampleSerializer::writeValue(std::vector<uint8_t>& buffer, const T& value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template<typename T>
T TrainingSampleSerializer::readValue(const uint8_t*& ptr) {
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

void TrainingSampleSerializer::serializeTensor(std::vector<uint8_t>& buffer, const torch::Tensor& tensor) {
    // Write tensor metadata
    uint8_t has_tensor = tensor.defined() ? 1 : 0;
    writeValue(buffer, has_tensor);

    if (!tensor.defined()) {
        return;
    }

    // Write number of dimensions
    int64_t ndim = tensor.dim();
    writeValue(buffer, ndim);

    // Write shape
    for (int64_t i = 0; i < ndim; ++i) {
        int64_t size = tensor.size(i);
        writeValue(buffer, size);
    }

    // Write dtype (simplified - assuming float32)
    uint8_t dtype = 0; // 0 for float32, extend as needed
    if (tensor.dtype() == torch::kFloat64) dtype = 1;
    else if (tensor.dtype() == torch::kInt32) dtype = 2;
    else if (tensor.dtype() == torch::kInt64) dtype = 3;
    writeValue(buffer, dtype);

    // Write tensor data
    torch::Tensor contiguous = tensor.contiguous().cpu();
    size_t num_bytes = contiguous.numel() * contiguous.element_size();
    writeValue(buffer, num_bytes);

    const uint8_t* data_ptr = static_cast<const uint8_t*>(contiguous.data_ptr());
    buffer.insert(buffer.end(), data_ptr, data_ptr + num_bytes);
}

torch::Tensor TrainingSampleSerializer::deserializeTensor(const uint8_t*& ptr, const uint8_t* end) {
    uint8_t has_tensor = readValue<uint8_t>(ptr);

    if (!has_tensor) {
        return torch::Tensor();
    }

    // Read dimensions
    int64_t ndim = readValue<int64_t>(ptr);
    std::vector<int64_t> shape(ndim);

    for (int64_t i = 0; i < ndim; ++i) {
        shape[i] = readValue<int64_t>(ptr);
    }

    // Read dtype
    uint8_t dtype = readValue<uint8_t>(ptr);
    torch::ScalarType scalar_type = torch::kFloat32;
    if (dtype == 1) scalar_type = torch::kFloat64;
    else if (dtype == 2) scalar_type = torch::kInt32;
    else if (dtype == 3) scalar_type = torch::kInt64;

    // Read tensor data
    size_t num_bytes = readValue<size_t>(ptr);

    if (ptr + num_bytes > end) {
        throw std::runtime_error("Buffer overflow in tensor deserialization");
    }

    // Create tensor from data
    auto options = torch::TensorOptions().dtype(scalar_type);
    torch::Tensor tensor = torch::empty(shape, options);
    std::memcpy(tensor.data_ptr(), ptr, num_bytes);
    ptr += num_bytes;

    return tensor;
}

void TrainingSampleSerializer::writeVector(std::vector<uint8_t>& buffer, const std::vector<float>& vec) {
    uint32_t size = static_cast<uint32_t>(vec.size());
    writeValue(buffer, size);

    for (float val : vec) {
        writeValue(buffer, val);
    }
}

void TrainingSampleSerializer::writeVector(std::vector<uint8_t>& buffer, const std::vector<int>& vec) {
    uint32_t size = static_cast<uint32_t>(vec.size());
    writeValue(buffer, size);

    for (int val : vec) {
        writeValue(buffer, val);
    }
}

std::vector<float> TrainingSampleSerializer::readFloatVector(const uint8_t*& ptr) {
    uint32_t size = readValue<uint32_t>(ptr);
    std::vector<float> vec(size);

    for (uint32_t i = 0; i < size; ++i) {
        vec[i] = readValue<float>(ptr);
    }

    return vec;
}

std::vector<int> TrainingSampleSerializer::readIntVector(const uint8_t*& ptr) {
    uint32_t size = readValue<uint32_t>(ptr);
    std::vector<int> vec(size);

    for (uint32_t i = 0; i < size; ++i) {
        vec[i] = readValue<int>(ptr);
    }

    return vec;
}

std::string TrainingSampleSerializer::serialize(const TrainingSampleAdvantage& sample) {
    std::vector<uint8_t> buffer;

    // Reserve some space to avoid reallocations (estimate)
    buffer.reserve(1024 * 10);

    // Write version number for future compatibility
    uint32_t version = 1;
    writeValue(buffer, version);

    // Serialize InfoSet
    // Write number of card tensors
    uint32_t num_card_tensors = static_cast<uint32_t>(sample.infoset.cardTensors.size());
    writeValue(buffer, num_card_tensors);

    // Write each card tensor
    for (const auto& tensor : sample.infoset.cardTensors) {
        serializeTensor(buffer, tensor);
    }

    // Write bet tensor
    serializeTensor(buffer, sample.infoset.betTensor);

    // Write iteration
    writeValue(buffer, sample.iteration);

    // Write advantages vector
    writeVector(buffer, sample.advantages);

    // Write legal_action_indices vector
    writeVector(buffer, sample.legal_action_indices);

    // Write weight
    writeValue(buffer, sample.weight);

    // Convert buffer to string
    return std::string(buffer.begin(), buffer.end());
}

std::unique_ptr<TrainingSampleAdvantage> TrainingSampleSerializer::deserialize(const std::string& data) {
    if (data.empty()) {
        return nullptr;
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
    const uint8_t* end = ptr + data.size();

    // Read version
    uint32_t version = readValue<uint32_t>(ptr);
    if (version != 1) {
        throw std::runtime_error("Unsupported serialization version: " + std::to_string(version));
    }

    auto sample = std::make_unique<TrainingSampleAdvantage>();

    // Read InfoSet
    uint32_t num_card_tensors = readValue<uint32_t>(ptr);
    sample->infoset.cardTensors.reserve(num_card_tensors);

    for (uint32_t i = 0; i < num_card_tensors; ++i) {
        sample->infoset.cardTensors.push_back(deserializeTensor(ptr, end));
    }

    sample->infoset.betTensor = deserializeTensor(ptr, end);

    // Read other fields
    sample->iteration = readValue<int>(ptr);
    sample->advantages = readFloatVector(ptr);
    sample->legal_action_indices = readIntVector(ptr);
    sample->weight = readValue<float>(ptr);

    return sample;
}