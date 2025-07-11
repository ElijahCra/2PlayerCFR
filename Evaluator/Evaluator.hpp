//
// Created by elijah on 7/9/25.
//

#ifndef EVALUATOR_HPP
#define EVALUATOR_HPP
#include <ext/random>

#include "../Storage/NodeStorage.hpp"
template <typename GameType>
class Evaluator
{
public:
    Evaluator();
    void Evaluate(const CFR::NodeStorage& strat1, const CFR::NodeStorage& strat2, uint32_t iterations);
    void playGame(GameType& game, const CFR::NodeStorage& strat1, const CFR::NodeStorage& strat2);
private:
    std::mt19937 generator;
    std::array<float,2> utilitySums{};
};
template <typename GameType>
Evaluator<GameType>::Evaluator() : generator(std::random_device()())
{

}


template <typename GameType>
void Evaluator<GameType>::Evaluate(const CFR::NodeStorage& strat1, const CFR::NodeStorage& strat2, uint32_t iterations)
{
    for (uint32_t i = 0; i < iterations; ++i)
    {
        playGame(strat1,strat2);
    }
}

template <typename GameType>
void Evaluator<GameType>::playGame(GameType& game,const CFR::NodeStorage& strat1, const CFR::NodeStorage& strat2)
{
    if ("chance" == game.getType())
    {
        playGame(game.transition());
    }
    if ("terminal" == game.getType())
    {
        utilitySums[0] += game.getUtility(0);
        utilitySums[1] += game.getUtility(1);
    }
    if (game.getCurrentPlayer() == 0)
    {
        auto currentStrategy = strat1.getNode(game.getInfoSet(0))->getAverageStrategy();

        std::discrete_distribution<int> actionSpread(currentStrategy.begin(),currentStrategy.end());
        int actionChoice = actionSpread(generator);
        game.transition(game.getAvailActions()[actionChoice]);
    }
}

#endif //EVALUATOR_HPP
