add_library(CFR STATIC
        Node.cpp
        RegretMinimizer.hpp
        MultiThreadedTrainer.hpp
)
target_link_libraries(CFR PUBLIC Utility Storage)

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(CFR PUBLIC DEFAULT_DB_PATH="${CMAKE_SOURCE_DIR}/cfr_nodes_db")

