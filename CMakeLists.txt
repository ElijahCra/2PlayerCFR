cmake_minimum_required(VERSION 3.22)
project(2PlayerCFR LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)



add_subdirectory(Cards)
add_subdirectory(CFR)
add_subdirectory(Game)
add_subdirectory(Test)
add_subdirectory(Utility)
add_subdirectory(TwoPlusTwoHandEvaluator)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(2PlayerCFR main.cpp)

target_link_libraries(2PlayerCFR PUBLIC CFR Cards Utility Texas)

message(STATUS "Current binary directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Current source directory: ${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(build_all
        DEPENDS 2PlayerCFR generate_table)

add_dependencies(2PlayerCFR generate_table)



if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/HandRanks.dat")
    message(STATUS "No Handrank")

    #target_link_libraries(generate_table PRIVATE math)

    add_custom_command(
            TARGET 2PlayerCFR POST_BUILD
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/TwoPlusTwoHandEvaluator/generate_table
    )

    add_custom_command(
            TARGET 2PlayerCFR POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/HandRanks.dat
            ${CMAKE_CURRENT_SOURCE_DIR}/HandRanks.dat
    )

endif ()