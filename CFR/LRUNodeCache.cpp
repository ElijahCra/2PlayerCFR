#include "LRUNodeCache.hpp"
#include <atomic>

namespace CFR {

LRUNodeCache::LRUNodeCache(size_t capacity, EvictionCallback evictionCallback)
    : capacity_(capacity), evictionCallback_(std::move(evictionCallback)) {
    if (capacity_ == 0) {
        throw std::invalid_argument("Cache capacity must be greater than 0");
    }
}

std::shared_ptr<Node> LRUNodeCache::getNode(const std::string& infoSet) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cacheMap_.find(infoSet);
    if (it == cacheMap_.end()) {
        misses_.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    
    hits_.fetch_add(1, std::memory_order_relaxed);
    
    // Move accessed item to front
    moveToFront(it->second);
    
    return it->second->node;
}

void LRUNodeCache::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cacheMap_.find(infoSet);
    if (it != cacheMap_.end()) {
        // Update existing entry
        it->second->node = std::move(node);
        moveToFront(it->second);
        return;
    }
    
    // Add new entry
    if (cacheList_.size() >= capacity_) {
        evictLRU();
    }
    
    cacheList_.emplace_front(infoSet, std::move(node));
    cacheMap_[infoSet] = cacheList_.begin();
}

bool LRUNodeCache::hasNode(const std::string& infoSet) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return cacheMap_.find(infoSet) != cacheMap_.end();
}

void LRUNodeCache::removeNode(const std::string& infoSet) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cacheMap_.find(infoSet);
    if (it == cacheMap_.end()) {
        return;
    }
    
    cacheList_.erase(it->second);
    cacheMap_.erase(it);
}

size_t LRUNodeCache::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return cacheList_.size();
}

void LRUNodeCache::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (evictionCallback_) {
        for (const auto& entry : cacheList_) {
            evictionCallback_(entry.key, entry.node);
        }
    }
    
    cacheList_.clear();
    cacheMap_.clear();
}

double LRUNodeCache::getHitRate() const {
    uint64_t h = hits_.load(std::memory_order_relaxed);
    uint64_t m = misses_.load(std::memory_order_relaxed);
    uint64_t total = h + m;
    
    return total > 0 ? static_cast<double>(h) / static_cast<double>(total) : 0.0;
}

void LRUNodeCache::resetStats() {
    hits_.store(0, std::memory_order_relaxed);
    misses_.store(0, std::memory_order_relaxed);
}

void LRUNodeCache::evictLRU() {
    if (cacheList_.empty()) {
        return;
    }
    
    auto& lastEntry = cacheList_.back();
    
    if (evictionCallback_) {
        evictionCallback_(lastEntry.key, lastEntry.node);
    }
    
    cacheMap_.erase(lastEntry.key);
    cacheList_.pop_back();
}

void LRUNodeCache::moveToFront(typename CacheList::iterator it) {
    if (it == cacheList_.begin()) {
        return; // Already at front
    }
    
    // Move to front
    cacheList_.splice(cacheList_.begin(), cacheList_, it);
}

} // namespace CFR