//
// Created by Claude Code on 6/20/25.
//

#include "MapNodeStorage.hpp"

namespace CFR {

std::shared_ptr<Node> MapNodeStorage::getNode(const std::string& infoSet) {
    auto it = m_nodeMap.find(infoSet);
    return (it != m_nodeMap.end()) ? it->second : nullptr;
}

void MapNodeStorage::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    m_nodeMap[infoSet] = node;
}

bool MapNodeStorage::hasNode(const std::string& infoSet) const {
    return m_nodeMap.find(infoSet) != m_nodeMap.end();
}

void MapNodeStorage::removeNode(const std::string& infoSet) {
    m_nodeMap.erase(infoSet);
}

size_t MapNodeStorage::size() const {
    return m_nodeMap.size();
}

void MapNodeStorage::clear() {
    m_nodeMap.clear();
}

} // namespace CFR