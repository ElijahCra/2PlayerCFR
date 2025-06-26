//
// Created by Elijah Crain on 6/19/25.
//

#ifndef HYBRIDNODESTORAGE_HPP
#define HYBRIDNODESTORAGE_HPP

#include <iostream>
#include "NodeStorage.hpp"
#include "LRUNodeCache.hpp"
#include "ShardedLRUCache.hpp"
#include "RocksDBNodeStorage.hpp"

namespace CFR {
/// @brief Hybrid storage combining in-memory cache and RocksDB on disk
/// @tparam CacheType The cache implementation to use (LRUNodeCache or ShardedLRUCache)
template<typename CacheType = LRUNodeCache>
class HybridNodeStorage : public NodeStorage {
public:
    /// @brief Constructor
    /// @param cacheCapacity Maximum number of nodes to keep in cache (for LRU) or per shard (for Sharded)
    /// @param dbPath Path to RocksDB database directory
    explicit HybridNodeStorage(size_t cacheCapacity = 100000, const std::string& dbPath = DEFAULT_DB_PATH);

    ~HybridNodeStorage() override;
    
    /// @brief Flush all cached data to disk
    void flush();

    // NodeStorage interface
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) const override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;

    /// @brief Get cache hit rate
    double getCacheHitRate() const;

    /// @brief Get cache statistics
    void printStats() const;

    /// @brief Flush cache to persistent storage
    void flushCache();

private:
    void onCacheEviction(const std::string& key, std::shared_ptr<Node> node);

    std::unique_ptr<CacheType> m_cache;
    std::unique_ptr<RocksDBNodeStorage> m_storage;
};

// Template implementation
template<typename CacheType>
HybridNodeStorage<CacheType>::HybridNodeStorage(size_t cacheCapacity, const std::string& dbPath) {
    // Create RocksDB storage first
    m_storage = std::make_unique<RocksDBNodeStorage>(dbPath);
    
    // Create cache with eviction callback
    auto evictionCallback = [this](const std::string& key, std::shared_ptr<Node> node) {
        this->onCacheEviction(key, node);
    };
    
    m_cache = std::make_unique<CacheType>(cacheCapacity, evictionCallback);
}

template<typename CacheType>
std::shared_ptr<Node> HybridNodeStorage<CacheType>::getNode(const std::string& infoSet) {
    // First check cache
    auto node = m_cache->getNode(infoSet);
    if (node) {
        return node;
    }
    
    // If not in cache, check persistent storage
    node = m_storage->getNode(infoSet);
    if (node) {
        // Promote to cache
        m_cache->putNode(infoSet, node);
    }
    
    return node;
}

template<typename CacheType>
void HybridNodeStorage<CacheType>::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    // Always put in cache first
    m_cache->putNode(infoSet, node);
}

template<typename CacheType>
bool HybridNodeStorage<CacheType>::hasNode(const std::string& infoSet) const {
    return m_cache->hasNode(infoSet) || m_storage->hasNode(infoSet);
}

template<typename CacheType>
void HybridNodeStorage<CacheType>::removeNode(const std::string& infoSet) {
    m_cache->removeNode(infoSet);
    m_storage->removeNode(infoSet);
}

template<typename CacheType>
size_t HybridNodeStorage<CacheType>::size() const {
    return m_cache->size() + m_storage->size();
}

template<typename CacheType>
void HybridNodeStorage<CacheType>::clear() {
    m_cache->clear();
    m_storage->clear();
}

template<typename CacheType>
double HybridNodeStorage<CacheType>::getCacheHitRate() const {
    return m_cache->getHitRate();
}

template<typename CacheType>
void HybridNodeStorage<CacheType>::printStats() const {
    std::cout << "Cache Hit Rate: " << (getCacheHitRate() * 100.0) << "%\n";
    std::cout << "Cache Size: " << m_cache->size() << " nodes\n";
    std::cout << "Storage Size: " << m_storage->size() << " nodes\n";
}

template<typename CacheType>
HybridNodeStorage<CacheType>::~HybridNodeStorage() {
    flush();
}

template<typename CacheType>
void HybridNodeStorage<CacheType>::flush() {
    m_cache->flush();
}

template<typename CacheType>
void HybridNodeStorage<CacheType>::flushCache() {
    flush();
}

template<typename CacheType>
void HybridNodeStorage<CacheType>::onCacheEviction(const std::string& key, std::shared_ptr<Node> node) {
    // Save evicted node to persistent storage
    m_storage->putNode(key, node);
}

} // namespace CFR

#endif //HYBRIDNODESTORAGE_HPP
