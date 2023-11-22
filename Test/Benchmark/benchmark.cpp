//
// Created by elijah on 10/9/2023.
//

#include <benchmark/benchmark.h>
#include "../../CFR/RegretMinimizer.hpp"
#include "../../Game/Texas/Game.hpp"
#include "../../Cards/HandAbstraction/hand_index.h"

static void BM_TrainIterations(benchmark::State& state) {
    CFR::RegretMinimizer<Texas::Game> Minimize{(std::random_device()())};
    for (auto _ : state)
        Minimize.Train(1);
}
BENCHMARK(BM_TrainIterations);

static void BM_CreateGame(benchmark::State& state) {
    auto rng = std::mt19937(std::random_device()());
    for (auto _ : state) {
        auto *game = new Texas::Game(rng);
    }
}
BENCHMARK(BM_CreateGame);

static void BM_TransitionRoot(benchmark::State& state) {
    auto rng = std::mt19937(std::random_device()());
    auto *game = new Texas::Game(rng);
    for (auto _ : state) {
        game->transition(Texas::Game::Action::None);
    }
}
BENCHMARK(BM_TransitionRoot);


static void BM_Reinitialize(benchmark::State& state) {
    auto rng = std::mt19937(std::random_device()());
    auto *game = new Texas::Game(rng);
    game->transition(Texas::Game::Action::None);
    game->transition(Texas::Game::Action::Call);
    for (auto _ : state) {
        game->reInitialize();
    }
}
BENCHMARK(BM_Reinitialize);

static void BM_GameCopy(benchmark::State& state) {
    auto rng = std::mt19937(std::random_device()());
    auto *game = new Texas::Game(rng);
    game->transition(Texas::Game::Action::None);
    game->transition(Texas::Game::Action::Call);
    for (auto _ : state) {
        Texas::Game gamecopy(*game);
    }
}
BENCHMARK(BM_GameCopy);

static void BM_HandAbstract1(benchmark::State& state) {
    uint8_t cards4[] ={2,3,1,1};
    hand_indexer_t river_indexer;
    hand_indexer_init(4, cards4, &river_indexer);
}

static void BM_HandAbstract2(benchmark::State& state) {}

