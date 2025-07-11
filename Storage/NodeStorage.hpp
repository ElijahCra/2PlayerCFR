//
// Created by elijah on 6/17/25.
//

#ifndef NODESTORAGE_HPP
#define NODESTORAGE_HPP

#include <memory>
#include <string>
#include "../CFR/Node.hpp"

namespace CFR {

/// @brief Abstract interface for node storage systems
class NodeStorage {
public:
    virtual ~NodeStorage() = default;

    /// @brief Get a node by information set key
    /// @param infoSet The information set string key
    /// @return Shared pointer to the node, nullptr if not found
    virtual std::shared_ptr<Node> getNode(const std::string& infoSet) = 0;

    /// @brief Store a node with the given information set key
    /// @param infoSet The information set string key
    /// @param node The node to store
    virtual void putNode(const std::string& infoSet, std::shared_ptr<Node> node) = 0;

    /// @brief Check if a node exists for the given information set
    /// @param infoSet The information set string key
    /// @return True if the node exists, false otherwise
    virtual bool hasNode(const std::string& infoSet) const = 0;

    /// @brief Remove a node from storage
    /// @param infoSet The information set string key
    virtual void removeNode(const std::string& infoSet) = 0;

    /// @brief Get the current size of the storage
    /// @return Number of nodes stored
    [[nodiscard]] virtual size_t size() const = 0;

    /// @brief Clear all nodes from storage
    virtual void clear() = 0;
};

} // namespace CFR
#endif //NODESTORAGE_HPP
