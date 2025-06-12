#include "NodeSerializer.hpp"
#include <stdexcept>

namespace CFR {

std::string NodeSerializer::serialize(const Node& node) {
    const auto& regretSum = node.getRegretSum();
    const auto& strategy = node.getStrategy();
    const auto& strategySum = node.getStrategySum();
    const auto& averageStrategy = node.getAverageStrategy();
    
    uint8_t actionNum = static_cast<uint8_t>(regretSum.size());
    
    // Calculate total size needed
    size_t totalSize = sizeof(SerializedNode) + 4 * actionNum * sizeof(float);
    
    std::string result(totalSize, '\0');
    char* ptr = result.data();
    
    // Write header
    SerializedNode header{actionNum};
    std::memcpy(ptr, &header, sizeof(SerializedNode));
    ptr += sizeof(SerializedNode);
    
    // Write regretSum
    std::memcpy(ptr, regretSum.data(), actionNum * sizeof(float));
    ptr += actionNum * sizeof(float);
    
    // Write strategy
    std::memcpy(ptr, strategy.data(), actionNum * sizeof(float));
    ptr += actionNum * sizeof(float);
    
    // Write strategySum
    std::memcpy(ptr, strategySum.data(), actionNum * sizeof(float));
    ptr += actionNum * sizeof(float);
    
    // Write averageStrategy
    std::memcpy(ptr, averageStrategy.data(), actionNum * sizeof(float));
    
    return result;
}

std::shared_ptr<Node> NodeSerializer::deserialize(const std::string& data) {
    if (data.size() < sizeof(SerializedNode)) {
        return nullptr;
    }
    
    const char* ptr = data.data();
    
    // Read header
    SerializedNode header;
    std::memcpy(&header, ptr, sizeof(SerializedNode));
    ptr += sizeof(SerializedNode);
    
    uint8_t actionNum = header.actionNum;
    
    // Validate size
    size_t expectedSize = sizeof(SerializedNode) + 4 * actionNum * sizeof(float);
    if (data.size() != expectedSize) {
        return nullptr;
    }
    
    // Create new node
    auto node = std::make_shared<Node>(actionNum);
    
    // Read and restore data
    std::vector<float> regretSum(actionNum);
    std::memcpy(regretSum.data(), ptr, actionNum * sizeof(float));
    ptr += actionNum * sizeof(float);
    
    // Skip strategy - it will be recalculated
    ptr += actionNum * sizeof(float);
    
    std::vector<float> strategySum(actionNum);
    std::memcpy(strategySum.data(), ptr, actionNum * sizeof(float));
    ptr += actionNum * sizeof(float);
    
    std::vector<float> averageStrategy(actionNum);
    std::memcpy(averageStrategy.data(), ptr, actionNum * sizeof(float));
    
    // Restore node state
    node->setRegretSum(regretSum);
    node->setStrategySum(strategySum);
    node->setAverageStrategy(averageStrategy);
    
    // Recalculate current strategy
    node->calcUpdatedStrategy();
    
    return node;
}

} // namespace CFR