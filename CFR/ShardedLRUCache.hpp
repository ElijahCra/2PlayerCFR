//
// Created by Elijah Crain on 6/19/25.
//

#ifndef SHARDEDLRUCACHE_HPP
#define SHARDEDLRUCACHE_HPP

#include <array>
#include <shared_mutex>
#include <functional>
#include <atomic>
#include "NodeStorage.hpp"
#include "LRUNodeCache.hpp"

namespace CFR {

/// @brief Thread-safe sharded LRU cache for multi-threaded access
/// Uses multiple LRU cache shards with fine-grained locking to minimize contention
class ShardedLRUCache : public NodeStorage {
public:
    /// @brief Number of shards (power of 2 for efficient modulo)
    static constexpr size_t NUM_SHARDS = 256;
    
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
    
    /// @brief Get total capacity across all shards
    size_t getTotalCapacity() const { return m_capacityPerShard * NUM_SHARDS; }
    
    /// @brief Get number of shards
    size_t getNumShards() const { return NUM_SHARDS; }

private:
    struct Shard {
        mutable std::shared_mutex mutex;
        LRUNodeCache cache;
        
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
    mutable std::atomic<uint64_t> m_totalHits{0};
    mutable std::atomic<uint64_t> m_totalMisses{0};
};

} // namespace CFR
#endif //SHARDEDLRUCACHE_HPP
