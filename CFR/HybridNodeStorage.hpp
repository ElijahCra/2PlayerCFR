//
// Created by Elijah Crain on 6/19/25.
//

#ifndef HYBRIDNODESTORAGE_HPP
#define HYBRIDNODESTORAGE_HPP

#include "NodeStorage.hpp"
#include "LRUNodeCache.hpp"
#include "RocksDBStorage.hpp"

namespace CFR {
/// @brief Hybrid storage combining in mem LRU cache and RocksDB on disk
class HybridNodeStorage : public NodeStorage {
public:
    /// @brief Constructor
    /// @param cacheCapacity Maximum number of nodes to keep in LRU cache
    /// @param dbPath Path to RocksDB database directory
    explicit HybridNodeStorage(size_t cacheCapacity = 100000, const std::string& dbPath = );

    ~HybridNodeStorage() override = default;

    // NodeStorage interface
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) override;
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

    std::unique_ptr<LRUNodeCache> m_cache;
    std::unique_ptr<RocksDBStorage> m_storage;
};

} // namespace CFR

#endif //HYBRIDNODESTORAGE_HPP
