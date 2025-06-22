//
// Created by Elijah Crain on 6/19/25.
//
#include "ShardedLRUCache.hpp"
#include <stdexcept>

namespace CFR {

ShardedLRUCache::ShardedLRUCache(size_t capacityPerShard, const EvictionCallback& evictionCallback)
    : capacityPerShard_(capacityPerShard) {
    if (capacityPerShard_ == 0) {
        throw std::invalid_argument("Cache capacity per shard must be greater than 0");
    }
    
    // Initialize all shards
    for (size_t i = 0; i < NUM_SHARDS; ++i) {
        shards_[i] = std::make_unique<Shard>(capacityPerShard_, evictionCallback);
    }
}

size_t ShardedLRUCache::getShardIndex(const std::string& key) const {
    return std::hash<std::string>{}(key) & (NUM_SHARDS - 1);
}

ShardedLRUCache::Shard& ShardedLRUCache::getShard(const std::string& key) {
    return *shards_[getShardIndex(key)];
}

const ShardedLRUCache::Shard& ShardedLRUCache::getShard(const std::string& key) const {
    return *shards_[getShardIndex(key)];
}

std::shared_ptr<Node> ShardedLRUCache::getNode(const std::string& infoSet) {
    auto& shard = getShard(infoSet);
    std::shared_lock<std::shared_mutex> lock(shard.mutex);
    
    auto result = shard.cache.getNode(infoSet);
    
    // Update global statistics
    if (result) {
        totalHits_.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_totalMisses.fetch_add(1, std::memory_order_relaxed);
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
    
    for (const auto& shardPtr : shards_) {
        std::shared_lock<std::shared_mutex> lock(shardPtr->mutex);
        totalSize += shardPtr->cache.size();
    }
    
    return totalSize;
}

void ShardedLRUCache::clear() {
    for (auto& shardPtr : shards_) {
        std::unique_lock<std::shared_mutex> lock(shardPtr->mutex);
        shardPtr->cache.clear();
    }
}

double ShardedLRUCache::getHitRate() const {
    uint64_t hits = totalHits_.load(std::memory_order_relaxed);
    uint64_t misses = m_totalMisses.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    
    return total > 0 ? static_cast<double>(hits) / static_cast<double>(total) : 0.0;
}

void ShardedLRUCache::resetStats() {
    totalHits_.store(0, std::memory_order_relaxed);
    m_totalMisses.store(0, std::memory_order_relaxed);
    
    // Reset individual shard statistics
    for (auto& shardPtr : shards_) {
        std::unique_lock<std::shared_mutex> lock(shardPtr->mutex);
        shardPtr->cache.resetStats();
    }
}

} // namespace CFR