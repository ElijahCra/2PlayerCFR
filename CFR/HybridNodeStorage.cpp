#include "HybridNodeStorage.hpp"
#include <iostream>

namespace CFR {

HybridNodeStorage::HybridNodeStorage(size_t cacheCapacity, const std::string& dbPath) {
    // Create RocksDB storage first
    storage_ = std::make_unique<RocksDBNodeStorage>(dbPath);
    
    // Create LRU cache with eviction callback
    auto evictionCallback = [this](const std::string& key, std::shared_ptr<Node> node) {
        this->onCacheEviction(key, node);
    };
    
    cache_ = std::make_unique<LRUNodeCache>(cacheCapacity, evictionCallback);
}

std::shared_ptr<Node> HybridNodeStorage::getNode(const std::string& infoSet) {
    // First check cache
    auto node = cache_->getNode(infoSet);
    if (node) {
        return node;
    }
    
    // If not in cache, check persistent storage
    node = storage_->getNode(infoSet);
    if (node) {
        // Promote to cache
        cache_->putNode(infoSet, node);
    }
    
    return node;
}

void HybridNodeStorage::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    // Always put in cache first
    cache_->putNode(infoSet, node);
    
    // The eviction callback will handle moving to persistent storage when needed
}

bool HybridNodeStorage::hasNode(const std::string& infoSet) {
    return cache_->hasNode(infoSet) || storage_->hasNode(infoSet);
}

void HybridNodeStorage::removeNode(const std::string& infoSet) {
    cache_->removeNode(infoSet);
    storage_->removeNode(infoSet);
}

size_t HybridNodeStorage::size() const {
    // Note: This may count some nodes twice if they're in both cache and storage
    // For exact count, we'd need to track unique keys
    return cache_->size() + storage_->size();
}

void HybridNodeStorage::clear() {
    cache_->clear();
    storage_->clear();
}

double HybridNodeStorage::getCacheHitRate() const {
    return cache_->getHitRate();
}

void HybridNodeStorage::printStats() const {
    std::cout << "Cache Hit Rate: " << (getCacheHitRate() * 100.0) << "%\n";
    std::cout << "Cache Size: " << cache_->size() << " nodes\n";
    std::cout << "Storage Size: " << storage_->size() << " nodes\n";
}

void HybridNodeStorage::flushCache() {
    // This would require iterating through all cache entries
    // For now, just clear the cache (eviction callback will save to storage)
    cache_->clear();
}

void HybridNodeStorage::onCacheEviction(const std::string& key, std::shared_ptr<Node> node) {
    // Save evicted node to persistent storage
    storage_->putNode(key, node);
}

} // namespace CFR