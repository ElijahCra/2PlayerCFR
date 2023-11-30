add_library(CFR STATIC Node.cpp RegretMinimizer.hpp)

target_link_libraries(CFR PUBLIC Utility Texas Cards)

target_include_directories(CFR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
