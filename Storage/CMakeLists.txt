add_library(Storage STATIC
        NodeStorage.hpp
        MapNodeStorage.hpp
        MapNodeStorage.cpp
        RocksDBNodeStorage.cpp
        NodeSerializer.cpp
        LRUNodeCache.hpp
        HybridNodeStorage.hpp
        LRUList.hpp
        ShardedLRUCache.hpp
)

find_package(rocksdb REQUIRED)

target_link_libraries(Storage PUBLIC RocksDB::rocksdb)

target_compile_definitions(Storage PUBLIC DEFAULT_DB_PATH="${CMAKE_SOURCE_DIR}/cfr_nodes_db")
