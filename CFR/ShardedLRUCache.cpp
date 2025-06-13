#include "ShardedLRUCache.hpp"

namespace CFR {

ShardedLRUCache::ShardedLRUCache(size_t totalCapacity, size_t numShards, 
                               LRUNodeCache::EvictionCallback evictionCallback)
    : numShards_(numShards) {
    
    size_t capacityPerShard = std::max(1UL, totalCapacity / numShards);
    shards_.reserve(numShards);
    
    for (size_t i = 0; i < numShards; ++i) {
        shards_.emplace_back(std::make_unique<LRUNodeCache>(capacityPerShard, evictionCallback));
    }
}

std::shared_ptr<Node> ShardedLRUCache::getNode(const std::string& infoSet) {
    return shards_[getShardIndex(infoSet)]->getNode(infoSet);
}

void ShardedLRUCache::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    shards_[getShardIndex(infoSet)]->putNode(infoSet, std::move(node));
}

bool ShardedLRUCache::hasNode(const std::string& infoSet) {
    return shards_[getShardIndex(infoSet)]->hasNode(infoSet);
}

void ShardedLRUCache::removeNode(const std::string& infoSet) {
    shards_[getShardIndex(infoSet)]->removeNode(infoSet);
}

size_t ShardedLRUCache::size() const {
    size_t total = 0;
    for (const auto& shard : shards_) {
        total += shard->size();
    }
    return total;
}

void ShardedLRUCache::clear() {
    for (auto& shard : shards_) {
        shard->clear();
    }
}

double ShardedLRUCache::getHitRate() const {
    double totalHits = 0;
    double totalRequests = 0;
    
    for (const auto& shard : shards_) {
        double hitRate = shard->getHitRate();
        size_t shardSize = shard->size();
        totalHits += hitRate * shardSize;
        totalRequests += shardSize;
    }
    
    return totalRequests > 0 ? totalHits / totalRequests : 0.0;
}

void ShardedLRUCache::resetStats() {
    for (auto& shard : shards_) {
        shard->resetStats();
    }
}

} // namespace CFR