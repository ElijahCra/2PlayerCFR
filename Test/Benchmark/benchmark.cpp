//
// Created by elijah on 10/9/2023.
//

#include <benchmark/benchmark.h>
#include "../../CFR/RegretMinimizer.hpp"
#include "../../Game/GameImpl/Texas/Game.hpp"
#include "../../Game/GameImpl/Preflop/Game.hpp"
#include "../../Utility/HandAbstraction/hand_index.h"

static void BM_TrainIterations(benchmark::State& state) {
    CFR::RegretMinimizer<Texas::Game> Minimize{(std::random_device()())};
    for (auto _ : state)
        Minimize.Train(100);
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
    uint8_t playerCards[]= {4,6,10,12,20,21,22};
    hand_indexer_t river_indexer;
    hand_indexer_init(4, cards4, &river_indexer);
    hand_index_last(&river_indexer, playerCards);
}

static void BM_HandAbstract2(benchmark::State& state) {}

