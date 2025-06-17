add_library(CFR STATIC Node.cpp RegretMinimizer.hpp
        NodeStorage.hpp
        RocksDBStorage.cpp
        RocksDBStorage.hpp)

target_link_libraries(CFR PUBLIC Utility)

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
