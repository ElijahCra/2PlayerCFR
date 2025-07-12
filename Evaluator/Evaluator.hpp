//
// Created by elijah on 7/9/25.
//

#ifndef EVALUATOR_HPP
#define EVALUATOR_HPP
#include <random>

#include "../Storage/NodeStorage.hpp"
template <typename GameType>
class Evaluator
{
public:
    Evaluator();
    void Evaluate(CFR::NodeStorage& strat1, CFR::NodeStorage& strat2, uint32_t iterations);
    std::pair<float,float> playGame(GameType& game, CFR::NodeStorage& stratPlayer0, CFR::NodeStorage& stratPlayer1);
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
        if (i % 2 == 0) {
            auto utility = playGame(game,strat1,strat2);
            utilitySums[0] += utility.first;
            utilitySums[1] += utility.second;
        } else {
            auto utility = playGame(game,strat2,strat1);
            utilitySums[0] += utility.second;
            utilitySums[1] += utility.first;
        }

    }
    std::cout << "Strat 1: "<< utilitySums[0]/1000 << "bb Strat 2: "<< utilitySums[1]/1000 <<"bb" <<std::endl;
}

template <typename GameType>
std::pair<float,float> Evaluator<GameType>::playGame(GameType& game,CFR::NodeStorage& stratPlayer0,CFR::NodeStorage& stratPlayer1)
{
    if ("chance" == game.getType())
    {
        game.transition(GameType::Action::None);
        return playGame(game,stratPlayer0,stratPlayer1);
    }
    if ("terminal" == game.getType())
    {
        return {game.getUtility(0), game.getUtility(1)};
    }

    //player action if we don't exit above
    CFR::NodeStorage* strategy;
    if (game.getCurrentPlayer() == 0) {
        strategy = &stratPlayer0;
    } else {
        strategy = &stratPlayer1;
    }
    auto node = strategy->getNode(game.getInfoSet(game.getCurrentPlayer()));
    std::vector<float> currentStrategy;
    if (node == nullptr)
    {
        node = std::make_shared<CFR::Node>(game.getActions().size());
        currentStrategy = node->getStrategy();
    } else
    {
        currentStrategy = node->getStrategy();
    }

    std::discrete_distribution<int> actionSpread(currentStrategy.begin(),currentStrategy.end());
    int actionChoice = actionSpread(generator);
    game.transition(game.getActions()[actionChoice]);
    return playGame(game,stratPlayer0,stratPlayer1);
}

#endif //EVALUATOR_HPP
