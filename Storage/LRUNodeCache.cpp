#include "LRUNodeCache.hpp"
#include <stdexcept>

namespace CFR {

LRUNodeCache::LRUNodeCache(size_t capacity, EvictionCallback evictionCallback)
    : m_capacity(capacity), m_evictionCallback(std::move(evictionCallback)), m_cacheList() {
    if (m_capacity == 0) {
        throw std::invalid_argument("Cache capacity must be greater than 0");
    }
}

std::shared_ptr<Node> LRUNodeCache::getNode(const std::string& infoSet) {
    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        m_misses.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    m_hits.fetch_add(1, std::memory_order_relaxed);

    // Move the accessed node to the front of the list
    m_cacheList.move_to_front(it->second.get_node());

    return it->second->node;
}

void LRUNodeCache::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    auto it = m_cacheMap.find(infoSet);
    if (it != m_cacheMap.end()) {
        // Update existing entry and move to front
        it->second->node = std::move(node);
        m_cacheList.move_to_front(it->second.get_node());
        return;
    }

    // Add new entry
    if (m_cacheMap.size() >= m_capacity) {
        evictLRU();
    }

    // Emplace creates a new list node and returns an iterator to it
    auto list_it = m_cacheList.emplace_front(infoSet, std::move(node));
    m_cacheMap[infoSet] = list_it;
}

bool LRUNodeCache::hasNode(const std::string& infoSet) const {
    return m_cacheMap.find(infoSet) != m_cacheMap.end();
}

void LRUNodeCache::removeNode(const std::string& infoSet) {
    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        return;
    }

    // Erase from the list, which handles unlinking and retirement
    m_cacheList.erase(it->second);
    m_cacheMap.erase(it);
}

size_t LRUNodeCache::size() const {
    return m_cacheMap.size();
}

void LRUNodeCache::clear() {
    if (m_evictionCallback) {
        for (const auto& pair : m_cacheMap) {
            m_evictionCallback(pair.first, pair.second->node);
        }
    }
    m_cacheList.clear();
    m_cacheMap.clear();
}

double LRUNodeCache::getHitRate() const {
    uint64_t h = m_hits.load(std::memory_order_relaxed);
    uint64_t m = m_misses.load(std::memory_order_relaxed);
    uint64_t total = h + m;

    return total > 0 ? static_cast<double>(h) / static_cast<double>(total) : 0.0;
}

void LRUNodeCache::resetStats() {
    m_hits.store(0, std::memory_order_relaxed);
    m_misses.store(0, std::memory_order_relaxed);
}

void LRUNodeCache::flush() {
    if (!m_evictionCallback) return;

    for (const auto& pair : m_cacheMap) {
        m_evictionCallback(pair.first, pair.second->node);
    }
}

void LRUNodeCache::evictLRU() {
    // pop_back is now a safe, atomic operation on the list.
    auto* evicted_node = m_cacheList.pop_back();

    if (evicted_node) {
        // Copy data before retiring the node
        std::string keyToEvict = evicted_node->value.key;
        std::shared_ptr<Node> nodeToEvict = evicted_node->value.node;

        // Erase from map
        m_cacheMap.erase(keyToEvict);

        // Retire the list node for safe memory reclamation
        evicted_node->retire();

        // Call the eviction callback if it exists
        if (m_evictionCallback) {
            m_evictionCallback(keyToEvict, nodeToEvict);
        }
    }
}

// This function is no longer needed as move_to_front is implemented in the list.
// void LRUNodeCache::moveToFront(typename CacheList::iterator it) { ... }

} // namespace CFR
