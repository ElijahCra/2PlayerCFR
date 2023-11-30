add_subdirectory(HandAbstraction)
add_library(Cards STATIC Cards.cpp)
target_include_directories(Cards PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Cards PUBLIC HandAbstraction)