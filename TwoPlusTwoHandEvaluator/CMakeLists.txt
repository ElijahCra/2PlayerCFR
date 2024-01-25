add_executable(generate_table
        generate_table.cpp
        mtrand.cpp
        pokerlib.cpp
)
target_include_directories(generate_table PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#add_library(TwoPlusTwoHandEvaluator generate_table.cpp)


