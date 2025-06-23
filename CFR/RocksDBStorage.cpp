//
// Created by elijah on 6/17/25.
//

#include "RocksDBStorage.hpp"

#include <iostream>
#include <utility>
#include <format>
#include <vector>
#include <rocksdb/table.h>
#include <rocksdb/filter_policy.h>
#include "NodeSerializer.hpp"

namespace CFR {

RocksDBStorage::RocksDBStorage(std::string  dbPath) : m_dbPath(std::move(dbPath)) {
    const rocksdb::Options options = getDefaultOptions();
    rocksdb::DB* db;

    if (rocksdb::Status status = rocksdb::DB::Open(options, m_dbPath, &db); !status.ok()) {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    m_db.reset(db);
    std::cout << std::format("DB Size: {}", this->size()) << std::endl;
}

RocksDBStorage::~RocksDBStorage() {
    if (m_db) {
        m_db->Close();
    }
}

std::shared_ptr<Node> RocksDBStorage::getNode(const std::string& infoSet) {

    if (!m_db) {
        return nullptr;
    }

    std::string value;
    rocksdb::Status status = m_db->Get(rocksdb::ReadOptions(), infoSet, &value);

    if (!status.ok()) {
        return nullptr;
    }

    return NodeSerializer::deserialize(value);
}
std::vector<std::shared_ptr<Node>> RocksDBStorage::multiGetNode(const std::vector<std::string>& infoSets)
{
    if (!m_db) {
        return {nullptr};
    }
    std::vector<rocksdb::Slice> keys;
    for (const auto& infoSet : infoSets) {
        keys.emplace_back(infoSet);
    }
    std::vector<std::string> values;
    std::vector<rocksdb::Status> status = m_db->MultiGet(rocksdb::ReadOptions(),keys,&values);

    std::vector<std::shared_ptr<Node>> nodes;
    for (const auto& value : values) {
        nodes.emplace_back(NodeSerializer::deserialize(value));
    }
    return nodes;

}

void RocksDBStorage::putNode(const std::string& infoSet, std::shared_ptr<Node> node) {

    if (!m_db || !node) {
        return;
    }

    std::string serialized = NodeSerializer::serialize(*node);
    rocksdb::Status status = m_db->Put(rocksdb::WriteOptions(), infoSet, serialized);

    if (!status.ok()) {
        throw std::runtime_error("Failed to put node: " + status.ToString());
    }
}
bool RocksDBStorage::hasNode(const std::string& infoSet) const {

    if (!m_db) {
        return false;
    }

    std::string value;
    rocksdb::Status status = m_db->Get(rocksdb::ReadOptions(), infoSet, &value);

    return status.ok();
}

void RocksDBStorage::removeNode(const std::string& infoSet) {
    if (!m_db) {
        return;
    }

    rocksdb::Status status = m_db->Delete(rocksdb::WriteOptions(), infoSet);

    if (!status.ok()) {
        throw std::runtime_error("Failed to remove node: " + status.ToString());
    }
}

size_t RocksDBStorage::size() const {
    if (!m_db) {
        return 0;
    }

    std::string value;
    m_db->GetProperty("rocksdb.estimate-num-keys", &value);
    return std::stoi(value);
}


void RocksDBStorage::clear() {
    if (!m_db) {
        return;
    }

    rocksdb::Iterator* it = m_db->NewIterator(rocksdb::ReadOptions());
    rocksdb::WriteBatch batch;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        batch.Delete(it->key());
    }

    delete it;

    rocksdb::Status status = m_db->Write(rocksdb::WriteOptions(), &batch);

    if (!status.ok()) {
        throw std::runtime_error("Failed to clear database: " + status.ToString());
    }
}

bool RocksDBStorage::isOpen() const {
    return m_db != nullptr;
}


std::string RocksDBStorage::getStats() const {
    if (!m_db) {
        return "Database not open";
    }

    std::string stats;
    m_db->GetProperty("rocksdb.stats", &stats);
    return stats;
}

void RocksDBStorage::compact() {
    if (!m_db) {
        return;
    }

    rocksdb::CompactRangeOptions options;
    m_db->CompactRange(options, nullptr, nullptr);
}


rocksdb::Options RocksDBStorage::getDefaultOptions() {
    rocksdb::Options options;

    // Create the DB if it doesn't exist
    options.create_if_missing = true;

    //Multithread options
    options.allow_concurrent_memtable_write = true;
    options.unordered_write = true;

    //Bloom filter or maybe ribbon?
    options.memtable_whole_key_filtering = true;

    // Optimize for point lookups
    options.OptimizeForPointLookup(64);

    // Set compression
    options.compression = rocksdb::kSnappyCompression;

    // Set memory budget
    options.write_buffer_size = 64 * 1024 * 1024; // 64MB
    options.max_write_buffer_number = 4;
    options.min_write_buffer_number_to_merge = 2;

    // Set level compaction
    options.level0_file_num_compaction_trigger = 4;
    options.level0_slowdown_writes_trigger = 20;
    options.level0_stop_writes_trigger = 36;
    options.max_bytes_for_level_base = 256 * 1024 * 1024;
    options.max_bytes_for_level_multiplier = 8;

    //Bloom filter
    rocksdb::BlockBasedTableOptions table_options;
    table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));


    // Set block cache
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    return options;
}

rocksdb::ReadOptions RocksDBStorage::getDefaultReadOptions()
{
    rocksdb::ReadOptions options;
    options.async_io = true;
    options.optimize_multiget_for_io = true;
    return options;
}
} // namespace CFR
