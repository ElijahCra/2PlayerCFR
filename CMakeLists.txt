cmake_minimum_required(VERSION 3.22)
project(2PlayerCFR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(Game)
add_subdirectory(Utility)
add_subdirectory(CFR)
add_subdirectory(Test)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(2PlayerCFR main.cpp)

target_include_directories(2PlayerCFR PUBLIC Utility Game CFR)
target_link_libraries(CFR PUBLIC Utility Game)
target_link_libraries(2PlayerCFR PUBLIC CFR)
