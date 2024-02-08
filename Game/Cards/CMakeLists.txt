add_subdirectory(HandAbstraction)
add_library(Cards STATIC Cards.cpp)
target_include_directories(Cards PUBLIC .)
target_link_libraries(Cards PUBLIC HandAbstraction)