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
    void Evaluate(CFR::NodeStorage& strat1, CFR::NodeStorage& strat2, uint32_t iterations);
    void playGame(GameType& game, CFR::NodeStorage& strat1, CFR::NodeStorage& strat2);
private:
    std::mt19937 generator;
    std::array<float,2> utilitySums{};
};
template <typename GameType>
Evaluator<GameType>::Evaluator() : generator(std::random_device()())
{

}


template <typename GameType>
void Evaluator<GameType>::Evaluate(CFR::NodeStorage& strat1, CFR::NodeStorage& strat2, uint32_t iterations)
{
    for (uint32_t i = 0; i < iterations; ++i)
    {
        generator();
        GameType game(generator);
        playGame(game,strat1,strat2);
    }
    std::cout << "Strat 1: "<< utilitySums[0]/1000 << "bb Strat 2: "<< utilitySums[1]/1000 <<"bb" <<std::endl;
}

template <typename GameType>
void Evaluator<GameType>::playGame(GameType& game,CFR::NodeStorage& strat1,CFR::NodeStorage& strat2)
{
    if ("chance" == game.getType())
    {
        game.transition(GameType::Action::None);
        playGame(game,strat1,strat2);
        return;
    }
    if ("terminal" == game.getType())
    {
        utilitySums[0] += game.getUtility(0);
        utilitySums[1] += game.getUtility(1);
        return;
    }
    if (game.getCurrentPlayer() == 0)
    {
        auto node = strat1.getNode(game.getInfoSet(0));
        std::vector<float> currentStrategy;
        if (node == nullptr)
        {
            node = std::make_shared<CFR::Node>(game.getActions().size());
            currentStrategy = node->getStrategy();
        } else
        {
            currentStrategy = node->getAverageStrategy();
        }

        std::discrete_distribution<int> actionSpread(currentStrategy.begin(),currentStrategy.end());
        int actionChoice = actionSpread(generator);
        game.transition(game.getActions()[actionChoice]);
        playGame(game,strat1,strat2);
        return;
    }

    if (game.getCurrentPlayer() == 1)
    {
        auto node = strat2.getNode(game.getInfoSet(1));
        if (node == nullptr)
        {
            node = std::make_shared<CFR::Node>(game.getActions().size());
        }
        auto currentStrategy = node->getAverageStrategy();
        std::discrete_distribution<int> actionSpread(currentStrategy.begin(),currentStrategy.end());
        int actionChoice = actionSpread(generator);
        game.transition(game.getActions()[actionChoice]);
        playGame(game,strat1,strat2);
        return;
    }

}

#endif //EVALUATOR_HPP
