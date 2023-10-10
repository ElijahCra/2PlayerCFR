//
// Created by elijah on 10/9/2023.
//

#include <benchmark/benchmark.h>
#include "../../CFR/RegretMinimizer.hpp"

static void BM_TrainIterations(benchmark::State& state) {
    CFR::RegretMinimizer<Texas::Game> Minimize{(std::random_device()())};
    for (auto _ : state)
        Minimize.Train(10);
}
// Register the function as a benchmark
BENCHMARK(BM_TrainIterations);