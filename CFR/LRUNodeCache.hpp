#ifndef INC_2PLAYERCFR_LRUNODECACHE_HPP
#define INC_2PLAYERCFR_LRUNODECACHE_HPP

#include <atomic>
#include <unordered_map>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <functional>
#include "NodeStorage.hpp"

namespace CFR {

/// @brief Thread-safe LRU cache for hot nodes
class LRUNodeCache : public NodeStorage {
public:
    /// @brief Callback function for evicted nodes
    using EvictionCallback = std::function<void(const std::string&, std::shared_ptr<Node>)>;
    
    /// @brief Constructor
    /// @param capacity Maximum number of nodes to keep in cache
    /// @param evictionCallback Optional callback when nodes are evicted
    explicit LRUNodeCache(size_t capacity, EvictionCallback evictionCallback = nullptr);
    
    ~LRUNodeCache() override = default;
    
    // NodeStorage interface
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;
    
    /// @brief Get current cache hit rate
    double getHitRate() const;
    
    /// @brief Reset hit/miss statistics
    void resetStats();

private:
    struct CacheEntry {
        std::string key;
        std::shared_ptr<Node> node;
        
        CacheEntry(std::string k, std::shared_ptr<Node> n) 
            : key(std::move(k)), node(std::move(n)) {}
    };
    
    using CacheList = std::list<CacheEntry>;
    using CacheMap = std::unordered_map<std::string, typename CacheList::iterator>;
    
    void evictLRU();
    void moveToFront(typename CacheList::iterator it);
    
    mutable std::shared_mutex mutex_;
    size_t capacity_;
    CacheList cacheList_;
    CacheMap cacheMap_;
    EvictionCallback evictionCallback_;
    
    // Statistics
    mutable std::atomic<uint64_t> hits_{0};
    mutable std::atomic<uint64_t> misses_{0};
};

} // namespace CFR

#endif //INC_2PLAYERCFR_LRUNODECACHE_HPP