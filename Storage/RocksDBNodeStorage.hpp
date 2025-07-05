//
// Created by elijah on 6/30/25.
//

#ifndef ROCKSDBNODESTORAGE_HPP
#define ROCKSDBNODESTORAGE_HPP

#include "NodeStorage.hpp"
#include <rocksdb/db.h>
#include <rocksdb/options.h>

namespace CFR {

/// @brief RocksDB-based persistent storage for cold nodes
class RocksDBNodeStorage final : public NodeStorage {
public:
    /// @brief Constructor
    /// @param dbPath Path to the RocksDB database directory
    explicit RocksDBNodeStorage(std::string  dbPath);

    ~RocksDBNodeStorage() override;

    // NodeStorage interface
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) const override;
    void removeNode(const std::string& infoSet) override;
    [[nodiscard]] size_t size() const override;
    void clear() override;

    /// @brief Check if the database is open
    [[nodiscard]] bool isOpen() const;

    /// @brief Get RocksDB statistics
    [[nodiscard]] std::string getStats() const;

private:
    std::unique_ptr<rocksdb::DB> m_db;
    std::string m_dbPath;

    [[nodiscard]] static rocksdb::Options getDefaultOptions();
    [[nodiscard]] static rocksdb::ReadOptions getDefaultReadOptions();
};

} // namespace CFR


#endif //ROCKSDBNODESTORAGE_HPP
