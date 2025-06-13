#ifndef SHARDED_LRU_CACHE_HPP
#define SHARDED_LRU_CACHE_HPP

#include "LRUNodeCache.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace CFR {

class ShardedLRUCache : public NodeStorage {
public:
    explicit ShardedLRUCache(size_t totalCapacity, size_t numShards = 16, 
                           LRUNodeCache::EvictionCallback evictionCallback = nullptr);
    
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;
    
    double getHitRate() const;
    void resetStats();

private:
    std::vector<std::unique_ptr<LRUNodeCache>> shards_;
    size_t numShards_;
    
    size_t getShardIndex(const std::string& infoSet) const {
        return std::hash<std::string>{}(infoSet) % numShards_;
    }
};

} // namespace CFR

#endif