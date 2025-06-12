#ifndef INC_2PLAYERCFR_ROCKSDNNODESTORAGE_HPP
#define INC_2PLAYERCFR_ROCKSDNNODESTORAGE_HPP

#include <memory>
#include <string>
#include <mutex>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include "NodeStorage.hpp"
#include "NodeSerializer.hpp"

namespace CFR {

/// @brief RocksDB-based persistent storage for cold nodes
class RocksDBNodeStorage : public NodeStorage {
public:
    /// @brief Constructor
    /// @param dbPath Path to the RocksDB database directory
    explicit RocksDBNodeStorage(const std::string& dbPath);
    
    ~RocksDBNodeStorage() override;
    
    // NodeStorage interface
    std::shared_ptr<Node> getNode(const std::string& infoSet) override;
    void putNode(const std::string& infoSet, std::shared_ptr<Node> node) override;
    bool hasNode(const std::string& infoSet) override;
    void removeNode(const std::string& infoSet) override;
    size_t size() const override;
    void clear() override;
    
    /// @brief Check if the database is open
    bool isOpen() const;
    
    /// @brief Get RocksDB statistics
    std::string getStats() const;
    
    /// @brief Manually compact the database
    void compact();

private:
    std::unique_ptr<rocksdb::DB> db_;
    mutable std::mutex mutex_;
    std::string dbPath_;
    
    rocksdb::Options getDefaultOptions() const;
};

} // namespace CFR

#endif //INC_2PLAYERCFR_ROCKSDNNODESTORAGE_HPP