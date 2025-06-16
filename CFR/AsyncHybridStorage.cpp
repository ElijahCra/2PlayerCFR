#include "AsyncHybridStorage.hpp"
#include <iostream>
#include <future>

namespace CFR {

AsyncHybridStorage::AsyncHybridStorage(size_t cacheCapacity,
                                     const std::string& dbPath,
                                     size_t numBackgroundThreads) {
    // Create RocksDB storage first
    storage_ = std::make_unique<RocksDBNodeStorage>(dbPath);
    
    // Create sharded cache with eviction callback
    auto evictionCallback = [this](const std::string& key, std::shared_ptr<Node> node) {
        this->onCacheEviction(key, node);
    };
    int perShardCapacity = std::ceil(static_cast<double>(cacheCapacity) / 64.0f);
    cache_ = std::make_unique<ShardedLRUCache>(perShardCapacity, evictionCallback);
    
    // Start background threads
    backgroundThreads_.reserve(numBackgroundThreads);
    for (size_t i = 0; i < numBackgroundThreads; ++i) {
        backgroundThreads_.emplace_back(&AsyncHybridStorage::backgroundWorker, this);
    }
}

AsyncHybridStorage::~AsyncHybridStorage() {
    // Signal shutdown
    shutdown_.store(true, std::memory_order_release);
    
    // Wake up all background threads
    evictionCV_.notify_all();
    loadCV_.notify_all();
    
    // Wait for all threads to finish
    for (auto& thread : backgroundThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

std::shared_ptr<Node> AsyncHybridStorage::getNode(const std::string& infoSet) {
    // First check cache - this is fast and lock-free for reads
    auto node = cache_->getNode(infoSet);
    if (node) {
        return node;
    }
    
    // Cache miss - need to check persistent storage
    totalCacheMisses_.fetch_add(1, std::memory_order_relaxed);
    
    // For now, do synchronous DB lookup to maintain interface compatibility
    // In a fully async design, this would queue a load operation
    node = storage_->getNode(infoSet);
    if (node) {
        // Promote to cache
        cache_->putNode(infoSet, node);
    }
    
    return node;
}

void AsyncHybridStorage::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    // Always put in cache first - this is fast
    cache_->putNode(infoSet, node);
    // Eviction callback will handle persistence asynchronously
}

bool AsyncHybridStorage::hasNode(const std::string& infoSet) const {
    // Check cache first
    if (cache_->hasNode(infoSet)) {
        return true;
    }
    
    // Fall back to storage check
    return storage_->hasNode(infoSet);
}

void AsyncHybridStorage::removeNode(const std::string& infoSet) {
    cache_->removeNode(infoSet);
    
    // Queue removal from persistent storage
    {
        std::lock_guard<std::mutex> lock(evictionMutex_);
        evictionQueue_.emplace(infoSet, nullptr); // nullptr signals deletion
    }
    evictionCV_.notify_one();
}

size_t AsyncHybridStorage::size() const {
    // This is approximate since cache and storage might have overlapping entries
    return cache_->size() + storage_->size();
}

void AsyncHybridStorage::clear() {
    cache_->clear();
    storage_->clear();
    
    // Clear any pending operations
    {
        std::lock_guard<std::mutex> lock(evictionMutex_);
        std::queue<EvictionOp> empty;
        evictionQueue_.swap(empty);
    }
    
    {
        std::lock_guard<std::mutex> lock(loadMutex_);
        std::queue<LoadOp> empty;
        loadQueue_.swap(empty);
    }
}

double AsyncHybridStorage::getCacheHitRate() const {
    return cache_->getHitRate();
}

void AsyncHybridStorage::printStats() {
    std::cout << "=== AsyncHybridStorage Stats ===\n";
    std::cout << "Cache Hit Rate: " << (getCacheHitRate() * 100.0) << "%\n";
    std::cout << "Cache Size: " << cache_->size() << " nodes\n";
    std::cout << "Storage Size: " << storage_->size() << " nodes\n";
    std::cout << "Total Cache Misses: " << totalCacheMisses_.load() << "\n";
    std::cout << "DB Loads Completed: " << dbLoadsCompleted_.load() << "\n";
    std::cout << "Evictions Completed: " << evictionsCompleted_.load() << "\n";
    std::cout << "Pending Operations: " << getPendingOpsCount() << "\n";
}

void AsyncHybridStorage::flushAndWait() {
    // Wait until all pending operations are processed
    while (getPendingOpsCount() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

size_t AsyncHybridStorage::getPendingOpsCount() {
    size_t count = 0;
    
    {
        std::lock_guard<std::mutex> lock(evictionMutex_);
        count += evictionQueue_.size();
    }
    
    {
        std::lock_guard<std::mutex> lock(loadMutex_);
        count += loadQueue_.size();
    }
    
    return count;
}

void AsyncHybridStorage::onCacheEviction(const std::string& key, std::shared_ptr<Node> node) {
    // Queue evicted node for background persistence
    // The shared_ptr copy here is intentional to ensure thread safety
    {
        std::lock_guard<std::mutex> lock(evictionMutex_);
        evictionQueue_.emplace(key, std::move(node));
    }
    evictionCV_.notify_one();
}

void AsyncHybridStorage::backgroundWorker() {
    while (!shutdown_.load(std::memory_order_acquire)) {
        // Process evictions
        processEvictions();
        
        // Process loads (if we implement async loading in the future)
        processLoads();
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    // Process remaining operations before shutdown
    processEvictions();
    processLoads();
}

void AsyncHybridStorage::processEvictions() {
    std::vector<EvictionOp> batch;
    
    // Get a batch of evictions
    {
        std::unique_lock<std::mutex> lock(evictionMutex_);
        
        // Wait for work or shutdown
        evictionCV_.wait(lock, [this] { 
            return !evictionQueue_.empty() || shutdown_.load(std::memory_order_acquire); 
        });
        
        // Batch up to 32 operations for efficiency
        constexpr size_t maxBatchSize = 32;
        while (!evictionQueue_.empty() && batch.size() < maxBatchSize) {
            batch.emplace_back(std::move(evictionQueue_.front()));
            evictionQueue_.pop();
        }
    }
    
    // Process the batch outside the lock
    for (auto& op : batch) {
        try {
            if (op.node) {
                // Save evicted node to persistent storage
                storage_->putNode(op.key, op.node);
            } else {
                // nullptr signals deletion
                storage_->removeNode(op.key);
            }
            evictionsCompleted_.fetch_add(1, std::memory_order_relaxed);
        } catch (const std::exception& e) {
            std::cerr << "Error processing eviction for key " << op.key << ": " << e.what() << "\n";
        }
    }
}

void AsyncHybridStorage::processLoads() {
    std::vector<LoadOp> batch;
    
    // Get a batch of load operations
    {
        std::unique_lock<std::mutex> lock(loadMutex_);
        
        // For now, this queue will be empty since we're doing synchronous loads
        // This is prepared for future async load implementation
        
        constexpr size_t maxBatchSize = 32;
        while (!loadQueue_.empty() && batch.size() < maxBatchSize) {
            batch.emplace_back(std::move(loadQueue_.front()));
            loadQueue_.pop();
        }
    }
    
    // Process the batch outside the lock
    for (auto& op : batch) {
        try {
            auto node = storage_->getNode(op.key);
            op.promise.set_value(node);
            dbLoadsCompleted_.fetch_add(1, std::memory_order_relaxed);
        } catch (const std::exception& e) {
            op.promise.set_exception(std::current_exception());
        }
    }
}

} // namespace CFR