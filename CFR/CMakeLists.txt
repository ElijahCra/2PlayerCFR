add_library(CFR STATIC
        Node.cpp
        RegretMinimizer.hpp
        NodeStorage.hpp
        MapNodeStorage.cpp
        RocksDBStorage.cpp
        NodeSerializer.cpp
        HybridNodeStorage.cpp
        LRUNodeCache.cpp
        ShardedLRUCache.cpp
)

target_link_libraries(CFR PUBLIC Utility)
target_link_libraries(CFR PUBLIC Utility ${ROCKSDB_LIBRARIES})

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(CFR PUBLIC DEFAULT_DB_PATH="${CMAKE_SOURCE_DIR}/cfr_nodes_db")
