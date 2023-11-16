add_subdirectory(HandAbstraction)
add_library(Hand STATIC Cards.cpp)
target_include_directories(Hand PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Hand PUBLIC HandAbstraction)