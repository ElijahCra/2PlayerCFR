add_library(CFR STATIC Node.cpp RegretMinimizer.hpp
        NodeStorage.hpp
        MapNodeStorage.cpp
        MapNodeStorage.hpp
        RocksDBStorage.cpp
        RocksDBStorage.hpp
        NodeSerializer.cpp
        NodeSerializer.hpp
        HybridNodeStorage.cpp
        HybridNodeStorage.hpp
        LRUNodeCache.cpp
        LRUNodeCache.hpp
        ShardedLRUCache.hpp
)

target_link_libraries(CFR PUBLIC Utility)

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(CFR PUBLIC DEFAULT_DB_PATH="${CMAKE_SOURCE_DIR}/cfr_nodes_db")
