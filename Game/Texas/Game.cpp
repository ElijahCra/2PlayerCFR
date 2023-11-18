//
// Created by Elijah Crain on 8/27/23.
//

#include "GameBase.hpp"
#include "ConcreteGameStates.hpp"
#include <utility>
#include <unordered_map>
#include <stdexcept>
#include <cassert>



namespace Texas {

    Game::Game(std::mt19937 &engine) : RNG(engine),
                                       currentPlayer(0),
                                       raiseNum(0),
                                       deckCards(baseDeck),
                                       utilities({0.f}),
                                       infoSet({""}),
                                       winner(-1),
                                       type("chance"),
                                       averageUtility(0.f),
                                       averageUtilitySum(0.f),
                                       currentRound(0),
                                       prevAction(Action::None),
                                       playerStacks({100.f}){

        currentState = &ChanceState::getInstance();
        currentState->enter(*this, Action::None);
    }

    void Game::setState(GameState &newState, Action action) {
        currentState->exit(*this, action); //
        currentState = &newState;
        currentState->enter(*this, action);
    }

    void Game::transition(Action action) {
        auto Actions = getActions();
        assert(std::find(Actions.begin(),Actions.end(),action)!=Actions.end());
        currentState->transition(*this, action);
    }

    void Game::addMoney() { //preflop ante's in milliBigBlinds
        utilities[0] = -500;
        utilities[1] = -1000;
        utilities[2] = 1500;
    }

    void Game::addMoney(float amount) {
        utilities[currentPlayer] -= amount;
        utilities[2] += amount;
    }

    std::vector<GameBase::Action> Game::getActions() const noexcept{
        return availActions;
    }

    void Game::setActions(std::vector<GameBase::Action> actionVec) {
        availActions = std::move(actionVec);
    }

    float Game::getUtility(int payoffPlayer) const noexcept{

        if (3 == winner) {
            return utilities[2] / 2.f + utilities[payoffPlayer];
        } else if (payoffPlayer == winner) {
            return utilities[2] + utilities[payoffPlayer];
        } else if (-1 == winner) {
            throw std::logic_error("game winner not updated from initialization");
        } else {
            return utilities[payoffPlayer];
        }
    }

    void Game::updateInfoSet(Action action) {
        for (int i = 0; i < PlayerNum; ++i) {
            infoSet[i].append(actionToStr(action));
        }

    }

    void Game::updateInfoSet(int player, int card) {
        infoSet[player].append(cardIntToStr(card));
    }

    std::string Game::getInfoSet(int player) const noexcept {
        return infoSet[player];
    }

    std::string Game::cardIntToStr(int card) {
        if (card < 10) {
            return '0' + std::to_string(card);
        } else {
            return std::to_string(card);
        }
    }

     std::string Game::actionToStr(Action action) {
        static std::unordered_map<Action, std::string> converter = {
                {Action::Check, "Ch"},
                {Action::Fold, "Fo"},
                {Action::Call, "Ca"},
                {Action::Raise1, "Ra1"},
                {Action::Raise2, "Ra2"},
                {Action::Raise3, "Ra3"},
                {Action::Raise5, "Ra5"},
                {Action::Raise10, "Ra10"},
                {Action::Reraise2, "Re2"},
                {Action::Reraise4, "Re4"},
                {Action::Reraise6, "Re6"},
                {Action::Reraise10, "Re10"},
                {Action::Reraise20, "Re20"},
                {Action::AllIn, "AI"}
        };
        return converter[action];
    }

    void Game::updatePlayer() {
        currentPlayer = 1 - currentPlayer;
    }

    void Game::setType(std::string type1) {
        type = std::move(type1);
    }

    std::string Game::getType() const noexcept{
        return type;
    }

    void Game::reInitialize() {
        RNG();
        currentPlayer = 0;
        raiseNum = 0;
        for (int i = 0; i < PlayerNum; ++i) {
            infoSet[i] = "";
            utilities[i] = 0;
        }
        winner = -1;
        type = "chance";
        currentRound = 0;
        currentState = &ChanceState::getInstance();
        currentState->enter(*this, Action::None);

    }

    void Game::dealCards() {
        switch (currentRound) {
            case 0: {
                //deal cards
                for (int i = 0; i < Game::DeckCardNum; ++i) {
                    deckCards[i] = i + 1;
                }
                // shuffle cards
                std::shuffle(deckCards.begin(),deckCards.end(),RNG);

                //deal player cards
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    int card1 = deckCards[2 * player];
                    int card2 = deckCards[1 + 2 * player];
                    if (card1 > card2) {
                        updateInfoSet(player, card1);
                        updateInfoSet(player, card2);
                    } else {
                        updateInfoSet(player, card2);
                        updateInfoSet(player, card1);
                    }
                }
                //set allowable actions to transfer from this node
                setActions({Game::Action::None});

                //put money into the pot for small and big blind
                addMoney();
                break;
            }
            case 1: {
                //deal flop cards to both players
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    updateInfoSet(player, deckCards[2 * Game::PlayerNum]);
                    updateInfoSet(player, deckCards[2 * Game::PlayerNum + 1]);
                    updateInfoSet(player, deckCards[2 * Game::PlayerNum + 2]);
                }
                //set allowable actions to transfer from this node
                setActions({Game::Action::None});
                break;
            }
            case 2: {
                //deal turn card to both players
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    updateInfoSet(player, deckCards[2 * Game::PlayerNum + 3]);
                }
                //set allowable actions to transfer from this node
                setActions({Game::Action::None});
                break;
            }
            case 3: {
                //deal river card to both players
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    updateInfoSet(player, deckCards[2 * Game::PlayerNum + 4]);
                }
                //set allowable actions to transfer from this node
                setActions({Game::Action::None});
                break;
            }
        }
    }
}