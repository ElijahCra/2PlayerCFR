cmake_minimum_required(VERSION 3.23)
project(2PlayerCFR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(Game)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(2PlayerCFR main.cpp)
target_include_directories(Game PUBLIC)
target_link_libraries(2PlayerCFR Game)