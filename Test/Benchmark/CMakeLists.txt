set (benchmark_DIR  ../../libs/benchmark/build/)
find_package(benchmark)
IF(${benchmark_FOUND})
ELSE(${benchmark_FOUND})
    set (benchmark_DIR  ../benchmark/build/)
    find_package(benchmark REQUIRED)
ENDIF(${benchmark_FOUND})

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark
        Texas
        CFR
        Utility
        benchmark::benchmark_main
        )

