//
// Created by Elijah Crain on 6/19/25.
//

#ifndef LRUNODECACHE_HPP
#define LRUNODECACHE_HPP
#include <functional>
#include <list>

#include "NodeStorage.hpp"

namespace CFR {
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
    
    size_t m_capacity;
    CacheList m_cacheList;
    CacheMap m_cacheMap;
    EvictionCallback m_evictionCallback;
    
    uint64_t m_hits{0};
    uint64_t m_misses{0};
};

} // namespace CFR

#endif //LRUNODECACHE_HPP
