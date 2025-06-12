#ifndef INC_2PLAYERCFR_NODESERIALIZER_HPP
#define INC_2PLAYERCFR_NODESERIALIZER_HPP

#include <string>
#include <memory>
#include <cstring>
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

} // namespace CFR

#endif //INC_2PLAYERCFR_NODESERIALIZER_HPP