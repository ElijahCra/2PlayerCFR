#cmake_minimum_required(VERSION 3.22)
#project(test)

# GoogleTest requires at least C++14
#set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/f8d7d77c06936315286eb55f8de22cd23c188571.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


enable_testing()

add_executable(
        hello_test
        texastest.cpp
        prefloptest.cpp
)
target_link_libraries(
        hello_test PUBLIC
        GTest::gtest_main
        Texas
        Preflop
        CFR
        Utility
        #benchmark::benchmark
        #benchmark::benchmark_main
        HandAbstraction
)

include(GoogleTest)
gtest_discover_tests(hello_test)



