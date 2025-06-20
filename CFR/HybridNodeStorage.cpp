//
// Created by Elijah Crain on 6/19/25.
//

#include "HybridNodeStorage.hpp"

namespace CFR {

HybridNodeStorage::HybridNodeStorage(size_t cacheCapacity, const std::string& dbPath) {
    // Create RocksDB storage first
    storage_ = std::make_unique<RocksDBStorage>(dbPath);
    
    // Create LRU cache with eviction callback
    auto evictionCallback = [this](const std::string& key, std::shared_ptr<Node> node) {
        this->onCacheEviction(key, node);
    };
    
    m_cache = std::make_unique<LRUNodeCache>(cacheCapacity, evictionCallback);
}

std::shared_ptr<Node> HybridNodeStorage::getNode(const std::string& infoSet) {
    // First check cache
    auto node = m_cache->getNode(infoSet);
    if (node) {
        return node;
    }
    
    // If not in cache, check persistent storage
    node = storage_->getNode(infoSet);
    if (node) {
        // Promote to cache
        m_cache->putNode(infoSet, node);
    }
    
    return node;
}

void HybridNodeStorage::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    // Always put in cache first
    m_cache->putNode(infoSet, node);
}

bool HybridNodeStorage::hasNode(const std::string& infoSet) {
    return m_cache->hasNode(infoSet) || storage_->hasNode(infoSet);
}

void HybridNodeStorage::removeNode(const std::string& infoSet) {
    m_cache->removeNode(infoSet);
    storage_->removeNode(infoSet);
}

size_t HybridNodeStorage::size() const {
    return m_cache->size() + storage_->size();
}

void HybridNodeStorage::clear() {
    m_cache->clear();
    storage_->clear();
}

double HybridNodeStorage::getCacheHitRate() const {
    return m_cache->getHitRate();
}

void HybridNodeStorage::printStats() const {
    std::cout << "Cache Hit Rate: " << (getCacheHitRate() * 100.0) << "%\n";
    std::cout << "Cache Size: " << m_cache->size() << " nodes\n";
    std::cout << "Storage Size: " << storage_->size() << " nodes\n";
}

void HybridNodeStorage::flushCache() {
    // eviction callback will save to storage
    m_cache->clear();
}

void HybridNodeStorage::onCacheEviction(const std::string& key, std::shared_ptr<Node> node) {
    // Save evicted node to persistent storage
    storage_->putNode(key, node);
}

} // namespace CFR