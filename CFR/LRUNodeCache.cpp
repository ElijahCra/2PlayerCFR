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

    auto it = cacheMap_.find(infoSet);
    if (it == cacheMap_.end()) {
        misses_+=1;
        return nullptr;
    }
    
    hits_+=1;
    
    // Move accessed item to front
    moveToFront(it->second);
    
    return it->second->node;
}

void LRUNodeCache::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {

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
    return cacheMap_.find(infoSet) != cacheMap_.end();
}

void LRUNodeCache::removeNode(const std::string& infoSet) {
    auto it = cacheMap_.find(infoSet);
    if (it == cacheMap_.end()) {
        return;
    }
    
    cacheList_.erase(it->second);
    cacheMap_.erase(it);
}

size_t LRUNodeCache::size() const {
    return cacheList_.size();
}

void LRUNodeCache::clear() {
    if (evictionCallback_) {
        for (const auto& entry : cacheList_) {
            evictionCallback_(entry.key, entry.node);
        }
    }
    
    cacheList_.clear();
    cacheMap_.clear();
}

double LRUNodeCache::getHitRate() const {
    uint64_t h = hits_;
    uint64_t m = misses_;
    uint64_t total = h + m;
    
    return total > 0 ? static_cast<double>(h) / static_cast<double>(total) : 0.0;
}

void LRUNodeCache::resetStats() {
    hits_=0;
    misses_=0;
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
        return;
    }
    

    cacheList_.splice(cacheList_.begin(), cacheList_, it);
}

} // namespace CFR