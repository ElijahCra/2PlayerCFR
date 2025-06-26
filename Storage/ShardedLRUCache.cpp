//
// Created by Elijah Crain on 6/19/25.
//
#include "ShardedLRUCache.hpp"

#include <cmath>
#include <stdexcept>

namespace CFR {

ShardedLRUCache::ShardedLRUCache(size_t cacheCapacity, const EvictionCallback& evictionCallback)
    : m_capacityPerShard(std::ceil(cacheCapacity/NUM_SHARDS)) {
    if (m_capacityPerShard == 0) {
        throw std::invalid_argument("Cache capacity per shard must be greater than 0");
    }
    
    // Initialize all shards
    for (size_t i = 0; i < NUM_SHARDS; ++i) {
        m_shards[i] = std::make_unique<Shard>(m_capacityPerShard, evictionCallback);
    }
}

size_t ShardedLRUCache::getShardIndex(const std::string& key) const {
    return std::hash<std::string>{}(key) & (NUM_SHARDS - 1);
}

ShardedLRUCache::Shard& ShardedLRUCache::getShard(const std::string& key) {
    return *m_shards[getShardIndex(key)];
}

const ShardedLRUCache::Shard& ShardedLRUCache::getShard(const std::string& key) const {
    return *m_shards[getShardIndex(key)];
}

std::shared_ptr<Node> ShardedLRUCache::getNode(const std::string& infoSet) {
    auto& shard = getShard(infoSet);
    std::unique_lock<std::shared_mutex> lock(shard.mutex);
    
    auto result = shard.cache.getNode(infoSet);
    
    // Update global statistics
    if (result) {
        totalHits_.fetch_add(1, std::memory_order_relaxed);
    } else {
        totalMisses_.fetch_add(1, std::memory_order_relaxed);
    }
    
    return result;
}

void ShardedLRUCache::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    auto& shard = getShard(infoSet);
    std::unique_lock<std::shared_mutex> lock(shard.mutex);
    
    shard.cache.putNode(infoSet, std::move(node));
}

bool ShardedLRUCache::hasNode(const std::string& infoSet) const {
    const auto& shard = getShard(infoSet);
    std::shared_lock<std::shared_mutex> lock(shard.mutex);
    return shard.cache.hasNode(infoSet);
}

void ShardedLRUCache::removeNode(const std::string& infoSet) {
    auto& shard = getShard(infoSet);
    std::unique_lock<std::shared_mutex> lock(shard.mutex);
    
    shard.cache.removeNode(infoSet);
}

size_t ShardedLRUCache::size() const {
    size_t totalSize = 0;
    
    for (const auto& shardPtr : m_shards) {
        std::shared_lock<std::shared_mutex> lock(shardPtr->mutex);
        totalSize += shardPtr->cache.size();
    }
    
    return totalSize;
}

void ShardedLRUCache::clear() {
    for (auto& shardPtr : m_shards) {
        std::unique_lock<std::shared_mutex> lock(shardPtr->mutex);
        shardPtr->cache.clear();
    }
}

double ShardedLRUCache::getHitRate() const {
    uint64_t hits = totalHits_.load(std::memory_order_relaxed);
    uint64_t misses = totalMisses_.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    
    return total > 0 ? static_cast<double>(hits) / static_cast<double>(total) : 0.0;
}

void ShardedLRUCache::resetStats() {
    totalHits_.store(0, std::memory_order_relaxed);
    totalMisses_.store(0, std::memory_order_relaxed);
    
    // Reset individual shard statistics
    for (auto& shardPtr : m_shards) {
        std::unique_lock<std::shared_mutex> lock(shardPtr->mutex);
        shardPtr->cache.resetStats();
    }
}

void ShardedLRUCache::flush() {
    for (auto& shardPtr : m_shards) {
        std::shared_lock<std::shared_mutex> lock(shardPtr->mutex);
        shardPtr->cache.flush();
    }
}

} // namespace CFR