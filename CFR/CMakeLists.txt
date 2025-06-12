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

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
