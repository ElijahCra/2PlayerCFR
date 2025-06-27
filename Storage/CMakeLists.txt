add_library(Storage STATIC
        NodeStorage.hpp
        MapNodeStorage.cpp
        RocksDBNodeStorage.cpp
        NodeSerializer.cpp
        HybridNodeStorage.hpp
        LRUNodeCache.cpp
        ShardedLRUCache.cpp
)

find_package(glog REQUIRED)
find_package(folly REQUIRED)

target_link_libraries(Storage PUBLIC folly glog::glog)
