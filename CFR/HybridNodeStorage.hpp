#ifndef INC_2PLAYERCFR_HYBRIDNODESTORAGE_HPP
#define INC_2PLAYERCFR_HYBRIDNODESTORAGE_HPP

#include <memory>
#include <string>
#include "NodeStorage.hpp"
#include "LRUNodeCache.hpp"
#include "RocksDBNodeStorage.hpp"

namespace CFR {

/// @brief Hybrid storage combining LRU cache for hot nodes and RocksDB for cold nodes
class HybridNodeStorage : public NodeStorage {
public:
    /// @brief Constructor
    /// @param cacheCapacity Maximum number of nodes to keep in LRU cache
    /// @param dbPath Path to RocksDB database directory
    explicit HybridNodeStorage(size_t cacheCapacity, const std::string& dbPath);
    
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
    
    std::unique_ptr<LRUNodeCache> cache_;
    std::unique_ptr<RocksDBNodeStorage> storage_;
};

} // namespace CFR

#endif //INC_2PLAYERCFR_HYBRIDNODESTORAGE_HPP