add_library(TexasCards STATIC TexasCards.cpp)
target_include_directories(TexasCards PUBLIC .)
target_link_libraries(TexasCards PUBLIC HandAbstraction)