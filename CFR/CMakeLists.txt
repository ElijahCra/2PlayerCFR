add_library(CFR STATIC Node.cpp RegretMinimizer.cpp)

target_link_libraries(CFR PUBLIC Utility Preflop Texas)

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
