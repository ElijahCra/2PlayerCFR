//
// Created by elijah on 6/17/25.
//

#include "RocksDBStorage.hpp"

#include <utility>

namespace CFR {

RocksDBStorage::RocksDBStorage(std::string  dbPath) : m_dbPath(std::move(dbPath)) {
    const rocksdb::Options options = getDefaultOptions();
    rocksdb::DB* db;

    if (rocksdb::Status status = rocksdb::DB::Open(options, m_dbPath, &db); !status.ok()) {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    m_db.reset(db);
}

RocksDBStorage::~RocksDBStorage() {
    if (m_db) {
        m_db->Close();
    }
}

} // namespace CFR
