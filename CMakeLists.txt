cmake_minimum_required(VERSION 3.23)
project(2PlayerCFR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(Game)
target_include_directories(Game PUBLIC)


add_executable(2PlayerCFR main.cpp)
