//
// Created by Elijah on 6/20/25.
//

#ifndef MAPNODESTORAGE_HPP
#define MAPNODESTORAGE_HPP

#include "NodeStorage.hpp"
#include <unordered_map>

namespace CFR {

/// @brief Simple in-memory storage using unordered_map
class MapNodeStorage : public NodeStorage {
public:
    MapNodeStorage() = default;
    ~MapNodeStorage() override = default;

    // NodeStorage interface
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) const override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;
    void flushCache(){}

private:
    std::unordered_map<std::string, std::shared_ptr<Node>> m_nodeMap;
};

} // namespace CFR

#endif //MAPNODESTORAGE_HPP