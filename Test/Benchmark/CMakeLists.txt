include(FetchContent)
FetchContent_Declare(
        benchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)


enable_testing()



#set (benchmark_DIR  ../../libs/benchmark/build/)
#find_package(benchmark)
#IF(${benchmark_FOUND})
#ELSE(${benchmark_FOUND})
#    set (benchmark_DIR  ../benchmark/build/)
#    find_package(benchmark REQUIRED)
#ENDIF(${benchmark_FOUND})
#
add_executable(benchmarkmain benchmark.cpp)
target_link_libraries(benchmarkmain
        Texas
        CFR
        Utility
        benchmark::benchmark_main
        )

