//
// Created by elijah on 6/17/25.
//

#ifndef ROCKSDBSTORAGE_HPP
#define ROCKSDBSTORAGE_HPP
#include "NodeStorage.hpp"
#include <rocksdb/db.h>
#include <rocksdb/options.h>

namespace CFR {

/// @brief RocksDB-based persistent storage for cold nodes
class RocksDBStorage final : public NodeStorage {
public:
    /// @brief Constructor
    /// @param dbPath Path to the RocksDB database directory
    explicit RocksDBStorage(std::string  dbPath);

    ~RocksDBStorage() override;

    // NodeStorage interface
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    std::vector<std::shared_ptr<Node>> multiGetNode(const std::vector<const std::string&>& infoSets);
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) override;
    void removeNode(const std::string& infoSet) override;
    [[nodiscard]] size_t size() const override;
    void clear() override;

    /// @brief Check if the database is open
    [[nodiscard]] bool isOpen() const;

    /// @brief Get RocksDB statistics
    [[nodiscard]] std::string getStats() const;

    /// @brief Manually compact the database
    void compact();

private:
    std::unique_ptr<rocksdb::DB> m_db;
    std::string m_dbPath;

    [[nodiscard]] static rocksdb::Options getDefaultOptions();
    [[nodiscard]] static rocksdb::ReadOptions getDefaultReadOptions();
};

} // namespace CFR


#endif //ROCKSDBSTORAGE_HPP
