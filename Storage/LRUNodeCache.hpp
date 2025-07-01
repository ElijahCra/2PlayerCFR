//
// Created by elijah on 6/30/25.
//

#ifndef LRUNODECACHE_HPP
#define LRUNODECACHE_HPP
#include <atomic>
#include <functional>
#include <unordered_map>
#include <list>

#include "NodeStorage.hpp"



namespace CFR {
template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
class LRUNodeCache : public NodeStorage {
public:
    /// @brief Callback function for evicted nodes which probably means send them to disk
    using EvictionCallback = std::function<void(const std::string&, std::shared_ptr<Node>)>;

    /// @param capacity Maximum number of nodes to keep in cache
    /// @param evictionCallback Optional callback when nodes are evicted
    explicit LRUNodeCache(size_t capacity, EvictionCallback evictionCallback = nullptr);

    ~LRUNodeCache() override = default;

    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) const override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;

    /// @brief Get current cache hit rate
    double getHitRate() const;

    /// @brief Reset hit/miss statistics
    void resetStats();

    /// @brief Flush all cached nodes to disk using eviction callback
    void flush();

private:
    struct CacheEntry {
        std::string key;
        std::shared_ptr<Node> node;

        CacheEntry() = default;
        CacheEntry(std::string k, std::shared_ptr<Node> n)
            : key(std::move(k)), node(std::move(n)) {}
};

    void evictLRU();

    size_t m_capacity;
    CacheList<CacheEntry> m_cacheList{};
    CacheMap<std::string, typename CacheList<CacheEntry>::iterator> m_cacheMap{};
    EvictionCallback m_evictionCallback;

    std::atomic<uint64_t> m_hits{0};
    std::atomic<uint64_t> m_misses{0};
};



template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
LRUNodeCache<CacheMap,CacheList>::LRUNodeCache(size_t capacity, EvictionCallback evictionCallback)
    : m_capacity(capacity), m_evictionCallback(std::move(evictionCallback)) {
    if (m_capacity == 0) {
        throw std::invalid_argument("Cache capacity must be greater than 0");
    }
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
std::shared_ptr<Node> LRUNodeCache<CacheMap,CacheList>::getNode(const std::string& infoSet) {
    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        m_misses.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    m_hits.fetch_add(1, std::memory_order_relaxed);

    // Move the accessed node to the front of the list
    m_cacheList.move_to_front(it->second);

    return it->second->node;
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    auto it = m_cacheMap.find(infoSet);
    if (it != m_cacheMap.end()) {
        // Update existing entry and move to front
        it->second->node = std::move(node);
        m_cacheList.move_to_front(it->second);
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

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
bool LRUNodeCache<CacheMap,CacheList>::hasNode(const std::string& infoSet) const {
    return m_cacheMap.find(infoSet) != m_cacheMap.end();
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::removeNode(const std::string& infoSet) {
    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        return;
    }

    // Erase from the list, which handles unlinking and retirement
    m_cacheList.erase(it->second);
    m_cacheMap.erase(it);
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
size_t LRUNodeCache<CacheMap,CacheList>::size() const {
    return m_cacheMap.size();
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::clear() {
    if (m_evictionCallback) {
        for (const auto& pair : m_cacheMap) {
            m_evictionCallback(pair.first, pair.second->node);
        }
    }
    m_cacheList.clear();
    m_cacheMap.clear();
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
double LRUNodeCache<CacheMap,CacheList>::getHitRate() const {
    uint64_t h = m_hits.load(std::memory_order_relaxed);
    uint64_t m = m_misses.load(std::memory_order_relaxed);
    uint64_t total = h + m;

    return total > 0 ? static_cast<double>(h) / static_cast<double>(total) : 0.0;
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::resetStats() {
    m_hits.store(0, std::memory_order_relaxed);
    m_misses.store(0, std::memory_order_relaxed);
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::flush() {
    if (!m_evictionCallback) return;

    for (const auto& pair : m_cacheMap) {
        m_evictionCallback(pair.first, pair.second->node);
    }
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::evictLRU() {
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


} // namespace CFR

#endif //LRUNODECACHE_HPP
