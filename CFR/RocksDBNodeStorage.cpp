#include "RocksDBNodeStorage.hpp"
#include <stdexcept>
#include <rocksdb/statistics.h>

namespace CFR {

RocksDBNodeStorage::RocksDBNodeStorage(const std::string& dbPath) : dbPath_(dbPath) {
    rocksdb::Options options = getDefaultOptions();
    rocksdb::DB* db;
    rocksdb::Status status = rocksdb::DB::Open(options, dbPath_, &db);
    
    if (!status.ok()) {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }
    
    db_.reset(db);
}

RocksDBNodeStorage::~RocksDBNodeStorage() {
    if (db_) {
        db_->Close();
    }
}

std::shared_ptr<Node> RocksDBNodeStorage::getNode(const std::string& infoSet) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return nullptr;
    }
    
    std::string value;
    rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), infoSet, &value);
    
    if (!status.ok()) {
        return nullptr;
    }
    
    return NodeSerializer::deserialize(value);
}

void RocksDBNodeStorage::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_ || !node) {
        return;
    }
    
    std::string serialized = NodeSerializer::serialize(*node);
    rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), infoSet, serialized);
    
    if (!status.ok()) {
        throw std::runtime_error("Failed to put node: " + status.ToString());
    }
}

bool RocksDBNodeStorage::hasNode(const std::string& infoSet) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return false;
    }
    
    std::string value;
    rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), infoSet, &value);
    
    return status.ok();
}

void RocksDBNodeStorage::removeNode(const std::string& infoSet) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return;
    }
    
    rocksdb::Status status = db_->Delete(rocksdb::WriteOptions(), infoSet);
    
    if (!status.ok()) {
        throw std::runtime_error("Failed to remove node: " + status.ToString());
    }
}

size_t RocksDBNodeStorage::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return 0;
    }
    
    size_t count = 0;
    rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
    
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        ++count;
    }
    
    delete it;
    return count;
}

void RocksDBNodeStorage::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return;
    }
    
    rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
    rocksdb::WriteBatch batch;
    
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        batch.Delete(it->key());
    }
    
    delete it;
    
    rocksdb::Status status = db_->Write(rocksdb::WriteOptions(), &batch);
    
    if (!status.ok()) {
        throw std::runtime_error("Failed to clear database: " + status.ToString());
    }
}

bool RocksDBNodeStorage::isOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return db_ != nullptr;
}

std::string RocksDBNodeStorage::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return "Database not open";
    }
    
    std::string stats;
    db_->GetProperty("rocksdb.stats", &stats);
    return stats;
}

void RocksDBNodeStorage::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return;
    }
    
    rocksdb::CompactRangeOptions options;
    db_->CompactRange(options, nullptr, nullptr);
}

rocksdb::Options RocksDBNodeStorage::getDefaultOptions() const {
    rocksdb::Options options;
    
    // Create the DB if it doesn't exist
    options.create_if_missing = true;
    
    // Optimize for point lookups
    options.OptimizeForPointLookup(64);
    
    // Set compression
    options.compression = rocksdb::kSnappyCompression;
    
    // Set memory budget
    options.write_buffer_size = 64 * 1024 * 1024; // 64MB
    options.max_write_buffer_number = 2;
    
    // Set level compaction
    options.level0_file_num_compaction_trigger = 2;
    options.level0_slowdown_writes_trigger = 20;
    options.level0_stop_writes_trigger = 36;
    
    // Set block cache
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory());
    
    return options;
}

} // namespace CFR