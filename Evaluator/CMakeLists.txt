add_library(Evaluator STATIC Evaluator.hpp RandomStrategy.hpp)
target_include_directories(Evaluator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Evaluator PUBLIC Preflop Texas)
