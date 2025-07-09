//
// Created by elijah on 6/30/25.
//

#ifndef LRUNODECACHE_HPP
#define LRUNODECACHE_HPP
#include <atomic>
#include <functional>
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <mutex>

#include "NodeStorage.hpp"

namespace CFR {
    struct CacheEntry {
        std::string key;
        std::shared_ptr<Node> node;

        CacheEntry() = default;
        CacheEntry(std::string k, std::shared_ptr<Node> n)
            : key(std::move(k)), node(std::move(n)) {}
};
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

    std::shared_ptr<Node> getNodeSafe(const std::string& infoSet);
    void putNodeSafe(const std::string& infoSet, std::shared_ptr<Node> node);
    bool hasNodeSafe(const std::string& infoSet) const;
    void removeNodeSafe(const std::string& infoSet);
    void clearSafe();
    void flushSafe();


    size_t size() const override;
    void clear() override;

    /// @brief Get current cache hit rate
    double getHitRate() const;

    /// @brief Reset hit/miss statistics
    void resetStats();

    /// @brief Flush all cached nodes to disk using eviction callback
    void flush();

private:

    void evictLRU();
    size_t m_capacity;
    CacheList<CacheEntry> m_cacheList{};
    CacheMap<std::string, typename CacheList<CacheEntry>::iterator> m_cacheMap{};
    EvictionCallback m_evictionCallback;

    std::atomic<uint64_t> m_hits{0};
    std::atomic<uint64_t> m_misses{0};
    mutable std::shared_mutex m_mapMutex;
    mutable std::shared_mutex m_listMutex;
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
    //put node in front of list and its iterator into map
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

    m_cacheList.erase(it->second);
    m_cacheMap.erase(it);
}


template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
std::shared_ptr<Node> LRUNodeCache<CacheMap,CacheList>::getNodeSafe(const std::string& infoSet) {
    std::unique_lock sharedMapLock(m_mapMutex);
    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        m_misses.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    m_hits.fetch_add(1, std::memory_order_relaxed);

    // Move the accessed node to the front of the list
    std::unique_lock listLock(m_listMutex);
    m_cacheList.move_to_front(it->second);

    return it->second->node;
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::putNodeSafe(const std::string& infoSet, std::shared_ptr<Node> node) {
    std::unique_lock uniqueMapLock(m_mapMutex);
    auto it = m_cacheMap.find(infoSet);
    if (it != m_cacheMap.end()) {
        // Update existing entry and move to front
        it->second->node = std::move(node);
        std::unique_lock listMutex(m_listMutex);
        m_cacheList.move_to_front(it->second);
        return;
    }
    std::unique_lock listMutex(m_listMutex);
    // Add new entry
    if (m_cacheMap.size() >= m_capacity) {
        evictLRU();
    }
    //put node in front of list and its iterator into map

    auto list_it = m_cacheList.emplace_front(infoSet, std::move(node));

    m_cacheMap[infoSet] = list_it;
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
bool LRUNodeCache<CacheMap,CacheList>::hasNodeSafe(const std::string& infoSet) const {
    std::shared_lock lock(m_mapMutex);
    return m_cacheMap.find(infoSet) != m_cacheMap.end();
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::removeNodeSafe(const std::string& infoSet) {
    std::unique_lock mapLock(m_mapMutex);
    auto it = m_cacheMap.find(infoSet);
    if (it == m_cacheMap.end()) {
        return;
    }

    std::unique_lock listLock(m_listMutex);
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
        for (const auto& entry : m_cacheList) {
            m_evictionCallback(entry.key, entry.node);
        }
    }

    m_cacheList.clear();
    m_cacheMap.clear();
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::clearSafe() {
    if (m_evictionCallback) {
        std::shared_lock lock(m_listMutex);
        for (const auto& entry : m_cacheList) {
            m_evictionCallback(entry.key, entry.node);
        }
        lock.unlock();
    }

    std::unique_lock listMutex(m_listMutex);
    std::unique_lock uniqueLock(m_mapMutex);
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
void LRUNodeCache<CacheMap,CacheList>::evictLRU() {
    if (m_cacheList.empty()) {
        return;
    }

    auto& lastEntry = m_cacheList.back();

    if (m_evictionCallback) {
        m_evictionCallback(lastEntry.key, lastEntry.node);
    }

    m_cacheMap.erase(lastEntry.key);
    m_cacheList.pop_back();
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::flush() {
    if (!m_evictionCallback) return;

    for (const auto& pair : m_cacheMap) {
        m_evictionCallback(pair.first, pair.second->node);
    }
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void LRUNodeCache<CacheMap,CacheList>::flushSafe() {
    if (!m_evictionCallback) return;

    std::shared_lock mapLock(m_mapMutex);
    for (const auto& pair : m_cacheMap) {
        m_evictionCallback(pair.first, pair.second->node);
    }
}
} // namespace CFR

#endif //LRUNODECACHE_HPP
