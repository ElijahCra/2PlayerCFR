//
// Created by Elijah Crain on 6/19/25.
//

#include "HybridNodeStorage.hpp"

#include <iostream>

namespace CFR {

HybridNodeStorage::HybridNodeStorage(size_t cacheCapacity, const std::string& dbPath) {
    // Create RocksDB storage first
    m_storage = std::make_unique<RocksDBStorage>(dbPath);
    
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
    node = m_storage->getNode(infoSet);
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
    return m_cache->hasNode(infoSet) || m_storage->hasNode(infoSet);
}

void HybridNodeStorage::removeNode(const std::string& infoSet) {
    m_cache->removeNode(infoSet);
    m_storage->removeNode(infoSet);
}

size_t HybridNodeStorage::size() const {
    return m_cache->size() + m_storage->size();
}

void HybridNodeStorage::clear() {
    m_cache->clear();
    m_storage->clear();
}

double HybridNodeStorage::getCacheHitRate() const {
    return m_cache->getHitRate();
}

void HybridNodeStorage::printStats() const {
    std::cout << "Cache Hit Rate: " << (getCacheHitRate() * 100.0) << "%\n";
    std::cout << "Cache Size: " << m_cache->size() << " nodes\n";
    std::cout << "Storage Size: " << m_storage->size() << " nodes\n";
}

void HybridNodeStorage::flushCache() {
    // eviction callback will save to storage
    m_cache->clear();
}

void HybridNodeStorage::onCacheEviction(const std::string& key, std::shared_ptr<Node> node) {
    // Save evicted node to persistent storage
    m_storage->putNode(key, node);
}

} // namespace CFR