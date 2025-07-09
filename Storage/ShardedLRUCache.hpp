//
// Created by elijah on 7/2/25.
//

#ifndef SHARDEDLRUCACHE_HPP
#define SHARDEDLRUCACHE_HPP
#include "LRUNodeCache.hpp"
#include "NodeStorage.hpp"

namespace CFR {
template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
class ShardedLRUCache : public NodeStorage
{
public:
     /// @brief Callback function for evicted nodes
    using EvictionCallback = std::function<void(const std::string&, std::shared_ptr<Node>)>;

    /// @brief Constructor
    /// @param cacheCapacity Maximum number of nodes in entire cache
    /// @param evictionCallback Optional callback when nodes are evicted
    explicit ShardedLRUCache(size_t cacheCapacity, const EvictionCallback& evictionCallback = nullptr);

    ~ShardedLRUCache() override = default;

    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) const override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;

    /// @brief Get current cache hit rate across all shards
    double getHitRate() const;

    /// @brief Reset hit/miss statistics across all shards
    void resetStats();

    /// @brief Flush all cached nodes to disk using eviction callback
    void flush();

    /// @brief Get total capacity across all shards
    size_t getTotalCapacity() const { return m_capacityPerShard * NUM_SHARDS; }

    /// @brief Get number of shards
    size_t getNumShards() const { return NUM_SHARDS; }

private:
    static constexpr size_t NUM_SHARDS = 32;

    struct Shard {
        LRUNodeCache<CacheMap,CacheList> cache;

        Shard(size_t capacity, EvictionCallback callback)
            : cache(capacity, std::move(callback)) {}
    };

    /// @brief Get shard index for a given key using hash
    size_t getShardIndex(const std::string& key) const;

    /// @brief Get the shard for a given key
    Shard& getShard(const std::string& key);
    const Shard& getShard(const std::string& key) const;

    size_t m_capacityPerShard;
    std::array<std::unique_ptr<Shard>, NUM_SHARDS> m_shards;

    // Global statistics
    mutable std::atomic<uint64_t> totalHits_{0};
    mutable std::atomic<uint64_t> totalMisses_{0};
};

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
ShardedLRUCache<CacheMap,CacheList>::ShardedLRUCache(size_t cacheCapacity, const EvictionCallback& evictionCallback)
    : m_capacityPerShard(std::ceil(cacheCapacity/NUM_SHARDS)) {
    if (m_capacityPerShard == 0) {
        throw std::invalid_argument("Cache capacity per shard must be greater than 0");
    }

    // Initialize all shards
    for (size_t i = 0; i < NUM_SHARDS; ++i) {
        m_shards[i] = std::make_unique<Shard>(m_capacityPerShard, evictionCallback);
    }
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
size_t ShardedLRUCache<CacheMap,CacheList>::getShardIndex(const std::string& key) const {
    return std::hash<std::string>{}(key) & (NUM_SHARDS - 1);
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
ShardedLRUCache<CacheMap,CacheList>::Shard&ShardedLRUCache<CacheMap,CacheList>::getShard(const std::string& key) {
    return *m_shards[getShardIndex(key)];
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
const ShardedLRUCache<CacheMap,CacheList>::Shard&ShardedLRUCache<CacheMap,CacheList>::getShard(const std::string& key) const {
    return *m_shards[getShardIndex(key)];
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
std::shared_ptr<Node> ShardedLRUCache<CacheMap,CacheList>::getNode(const std::string& infoSet) {
    auto& shard = getShard(infoSet);

    auto result = shard.cache.getNodeSafe(infoSet);

    // Update global statistics
    if (result) {
        totalHits_.fetch_add(1, std::memory_order_relaxed);
    } else {
        totalMisses_.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void ShardedLRUCache<CacheMap,CacheList>::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    auto& shard = getShard(infoSet);

    shard.cache.putNodeSafe(infoSet, std::move(node));
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
bool ShardedLRUCache<CacheMap,CacheList>::hasNode(const std::string& infoSet) const {
    const auto& shard = getShard(infoSet);
    return shard.cache.hasNodeSafe(infoSet);
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void ShardedLRUCache<CacheMap,CacheList>::removeNode(const std::string& infoSet) {
    auto& shard = getShard(infoSet);

    shard.cache.removeNodeSafe(infoSet);
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
size_t ShardedLRUCache<CacheMap,CacheList>::size() const {
    size_t totalSize = 0;

    for (const auto& shardPtr : m_shards) {
        totalSize += shardPtr->cache.size();
    }

    return totalSize;
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void ShardedLRUCache<CacheMap,CacheList>::clear() {
    for (auto& shardPtr : m_shards) {
        shardPtr->cache.clearSafe();
    }
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
double ShardedLRUCache<CacheMap,CacheList>::getHitRate() const {
    uint64_t hits = totalHits_.load(std::memory_order_relaxed);
    uint64_t misses = totalMisses_.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;

    return total > 0 ? static_cast<double>(hits) / static_cast<double>(total) : 0.0;
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void ShardedLRUCache<CacheMap,CacheList>::resetStats() {
    totalHits_.store(0, std::memory_order_relaxed);
    totalMisses_.store(0, std::memory_order_relaxed);

    // Reset individual shard statistics
    for (auto& shardPtr : m_shards) {
        shardPtr->cache.resetStats();
    }
}

template< template<typename mapKey, typename mapValue> typename CacheMap, template<typename CacheListObject> typename CacheList>
void ShardedLRUCache<CacheMap,CacheList>::flush() {
    for (auto& shardPtr : m_shards) {
        shardPtr->cache.flushSafe();
    }
}

} // namespace CFR
#endif //SHARDEDLRUCACHE_HPP
