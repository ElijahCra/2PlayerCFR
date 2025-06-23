//
// Created by Elijah Crain on 6/19/25.
//

#include "LRUNodeCache.hpp"

namespace CFR {

LRUNodeCache::LRUNodeCache(size_t capacity, EvictionCallback evictionCallback)
    : m_capacity(capacity), m_evictionCallback(std::move(evictionCallback)) {
    if (m_capacity == 0) {
        throw std::invalid_argument("Cache capacity must be greater than 0");
    }
}

std::shared_ptr<Node> LRUNodeCache::getNode(const std::string& infoSet) {

    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        m_misses+=1;
        return nullptr;
    }

    m_hits+=1;

    // Move accessed item to front
    moveToFront(it->second);

    return it->second->node;
}

void LRUNodeCache::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {

    auto it = m_cacheMap.find(infoSet);
    if (it != m_cacheMap.end()) {
        // Update existing entry
        it->second->node = std::move(node);
        moveToFront(it->second);
        return;
    }

    // Add new entry
    if (m_cacheList.size() >= m_capacity) {
        evictLRU();
    }

    m_cacheList.emplace_front(infoSet, std::move(node));
    m_cacheMap[infoSet] = m_cacheList.begin();
}

bool LRUNodeCache::hasNode(const std::string& infoSet) const {
    return m_cacheMap.find(infoSet) != m_cacheMap.end();
}

void LRUNodeCache::removeNode(const std::string& infoSet) {
    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        return;
    }

    m_cacheList.erase(it->second);
    m_cacheMap.erase(it);
}

size_t LRUNodeCache::size() const {
    return m_cacheList.size();
}

void LRUNodeCache::clear() {
    if (m_evictionCallback) {
        for (const auto& entry : m_cacheList) {
            m_evictionCallback(entry.key, entry.node);
        }
    }

    m_cacheList.clear();
    m_cacheMap.clear();
}

double LRUNodeCache::getHitRate() const {
    uint64_t h = m_hits;
    uint64_t m = m_misses;
    uint64_t total = h + m;
    
    return total > 0 ? static_cast<double>(h) / static_cast<double>(total) : 0.0;
}

void LRUNodeCache::resetStats() {
    m_hits=0;
    m_misses=0;
}

void LRUNodeCache::flush() {
    if (m_evictionCallback) {
        for (const auto& entry : m_cacheList) {
            m_evictionCallback(entry.key, entry.node);
        }
    }
}

void LRUNodeCache::evictLRU() {
    if (m_cacheList.empty()) {
        return;
    }

    // Make copies BEFORE any modifications to avoid reentrancy issues
    const auto& backEntry = m_cacheList.back();
    std::string keyToEvict = backEntry.key;
    std::shared_ptr<Node> nodeToEvict = backEntry.node;

    // Remove from cache first
    m_cacheMap.erase(keyToEvict);
    m_cacheList.pop_back();

    // Call eviction callback after removal
    if (m_evictionCallback) {
        m_evictionCallback(keyToEvict, nodeToEvict);
    }
}

void LRUNodeCache::moveToFront(typename CacheList::iterator it) {
    if (it == m_cacheList.begin()) {
        return;
    }
    
    m_cacheList.splice(m_cacheList.begin(), m_cacheList, it);
}

} // namespace CFR