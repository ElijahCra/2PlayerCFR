//
// Created by elijah on 6/18/25.
//

#ifndef NODESERIALIZER_HPP
#define NODESERIALIZER_HPP
#include <cstdint>
#include <memory>

#include "Node.hpp"

namespace CFR {

/// @brief Utility class for serializing and deserializing Node objects
class NodeSerializer {
public:
    /// @brief Serialize a Node to a binary string
    /// @param node The node to serialize
    /// @return Serialized binary data as string
    static std::string serialize(const Node& node);
    
    /// @brief Deserialize a Node from binary string
    /// @param data The serialized binary data
    /// @return Shared pointer to deserialized Node, nullptr on error
    static std::shared_ptr<Node> deserialize(const std::string& data);

private:
    struct SerializedNode {
        uint8_t actionNum;
        // Followed by actionNum * sizeof(float) bytes for each vector
        // Order: regretSum, strategy, strategySum, averageStrategy
    };
};

} // CFR

#endif //NODESERIALIZER_HPP
