add_library(CFR STATIC 
    Node.cpp 
    LRUNodeCache.cpp 
    NodeSerializer.cpp 
    RocksDBNodeStorage.cpp 
    HybridNodeStorage.cpp
    RegretMinimizer.hpp
)

# Find RocksDB
find_package(PkgConfig REQUIRED)
pkg_check_modules(ROCKSDB REQUIRED rocksdb)

target_link_libraries(CFR PUBLIC Utility ${ROCKSDB_LIBRARIES})
target_include_directories(CFR PRIVATE ${ROCKSDB_INCLUDE_DIRS})

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Define the default database path
target_compile_definitions(CFR PUBLIC DEFAULT_DB_PATH="${CMAKE_SOURCE_DIR}/shared_cfr_nodes_db")
