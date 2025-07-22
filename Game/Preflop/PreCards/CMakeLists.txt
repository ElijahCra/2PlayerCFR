add_library(PreCards STATIC PreCards.cpp)
target_include_directories(PreCards PUBLIC .)
target_link_libraries(PreCards PUBLIC HandAbstraction)