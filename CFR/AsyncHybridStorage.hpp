#ifndef INC_2PLAYERCFR_ASYNCHYBRIDSTORAGE_HPP
#define INC_2PLAYERCFR_ASYNCHYBRIDSTORAGE_HPP

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>

#include "NodeStorage.hpp"
#include "ShardedLRUCache.hpp"
#include "RocksDBNodeStorage.hpp"

namespace CFR {

/// @brief Async hybrid storage with background threads for RocksDB operations
/// Threads only interact with the cache, background threads handle persistence
class AsyncHybridStorage : public NodeStorage {
public:
    /// @brief Constructor
    /// @param cacheCapacity Total nodes across all shards
    /// @param dbPath Path to RocksDB database directory
    /// @param numBackgroundThreads Number of background threads for RocksDB operations
    explicit AsyncHybridStorage(size_t cacheCapacity,
                               const std::string& dbPath,
                               size_t numBackgroundThreads = 2);
    
    ~AsyncHybridStorage() override;
    
    // NodeStorage interface - these operations are fast and non-blocking
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) const override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;
    
    /// @brief Get cache hit rate
    double getCacheHitRate() const;
    
    /// @brief Get cache statistics
    void printStats();
    
    /// @brief Flush all pending operations and wait for completion
    void flushAndWait();
    
    /// @brief Get number of pending background operations
    size_t getPendingOpsCount();

private:
    struct EvictionOp {
        std::string key;
        std::shared_ptr<Node> node;
        
        EvictionOp(std::string k, std::shared_ptr<Node> n) 
            : key(std::move(k)), node(std::move(n)) {}
    };
    
    struct LoadOp {
        std::string key;
        std::promise<std::shared_ptr<Node>> promise;
        
        LoadOp(std::string k) : key(std::move(k)) {}
    };
    
    void onCacheEviction(const std::string& key, std::shared_ptr<Node> node);
    void backgroundWorker();
    void processEvictions();
    void processLoads();
    
    std::unique_ptr<ShardedLRUCache> cache_;
    std::unique_ptr<RocksDBNodeStorage> storage_;
    
    // Background thread management
    std::vector<std::thread> backgroundThreads_;
    std::atomic<bool> shutdown_{false};
    
    // Eviction queue - lock-free for producers, mutex for consumers
    std::queue<EvictionOp> evictionQueue_;
    std::mutex evictionMutex_;
    std::condition_variable evictionCV_;
    
    // Load queue for cache misses that need DB lookup
    std::queue<LoadOp> loadQueue_;
    std::mutex loadMutex_;
    std::condition_variable loadCV_;
    
    // Statistics
    mutable std::atomic<size_t> totalCacheMisses_{0};
    mutable std::atomic<size_t> dbLoadsCompleted_{0};
    mutable std::atomic<size_t> evictionsCompleted_{0};
};

} // namespace CFR

#endif //INC_2PLAYERCFR_ASYNCHYBRIDSTORAGE_HPP