add_subdirectory(TwoPlusTwoHandEvaluator)
add_library(Utility STATIC Utility.cpp)

target_compile_definitions(Utility PUBLIC PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
#target_include_directories(Utility PUBLIC .)

