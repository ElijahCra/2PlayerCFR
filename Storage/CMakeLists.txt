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

find_package(PkgConfig REQUIRED)
pkg_check_modules(ROCKSDB REQUIRED rocksdb)

find_package(glog REQUIRED)
find_package(folly REQUIRED)

target_link_libraries(Storage PUBLIC folly glog::glog ${ROCKSDB_LIBRARIES})
target_include_directories(Storage PRIVATE ${ROCKSDB_INCLUDE_DIRS})

target_compile_definitions(Storage PUBLIC DEFAULT_DB_PATH="${CMAKE_SOURCE_DIR}/cfr_nodes_db")
