//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include "../../Utility/Utility.hpp"

namespace Texas {

    void ChanceState::enter(Game &game, Game::Action action) {
        game.setType("chance");
        game.dealCards();
        game.raiseNum = 0;
    }

    void ChanceState::exit(Game &game, Game::Action action) {
        if (0 == game.currentRound) {
            game.currentPlayer = 0;
        } else {
            game.currentPlayer = 1;
        }
        game.prevAction = Game::Action::None;
    }

    void ChanceState::transition(Game &game, Game::Action action) {
        game.setState(ActionStateNoBet::getInstance(), Game::Action::None);
    }

    GameState &ChanceState::getInstance() {
        static ChanceState singleton;
        return singleton;
    }

    void ActionStateNoBet::enter(Game &game, Game::Action action) {
        if (0 == game.currentRound) {
            if (Game::Action::None == action) {
                game.setActions({Game::Action::Raise1, Game::Action::Call, Game::Action::Fold});
            } else if (Game::Action::Call == action) {
                game.setActions({Game::Action::Raise1, Game::Action::Check});
            }
        } else {
            game.setActions({Game::Action::Raise1, Game::Action::Check});
        }
        game.setType("action");
    }

    void ActionStateNoBet::transition(Game &game, Game::Action action) {
        if (Game::Action::Call == action) { //first action small-blind calls/limps
            game.setState(ActionStateNoBet::getInstance(), action);
        } else if (Game::Action::Check == action) {
            if (3 == game.currentRound) {
                if (Game::Action::Check == game.prevAction) {
                    game.setState(TerminalState::getInstance(), action);
                } else {
                    game.setState(ActionStateNoBet::getInstance(), action);
                    game.prevAction = Game::Action::Check;
                }
            } else if (0 == game.currentRound){
                game.setState(ChanceState::getInstance(), action);
            } else {
                if (Game::Action::Check == game.prevAction) {
                    game.setState(ChanceState::getInstance(), action);
                } else {
                    game.setState(ActionStateNoBet::getInstance(), action);
                    game.prevAction = Game::Action::Check;
                }
            }
        } else if (Game::Action::Fold == action) { //first action small-blind folds
            game.setState(TerminalState::getInstance(),
                           action);          // or second action bb checks -> post flop chance node
        } else if (Game::Action::Raise1 == action) { //first action small blind raises
            game.setState(ActionStateBet::getInstance(), action);
        }
    }

    GameState &ActionStateNoBet::getInstance() {
        static ActionStateNoBet singleton;
        return singleton;
    }

    void ActionStateNoBet::exit(Game &game, Game::Action action) {
        if (Game::Action::Call == action) {
            game.addMoney(500);
        } else if (Game::Action::Raise1 == action) {
            game.addMoney(1500);
            ++game.raiseNum;
        } else if (Game::Action::Check == action) {
            if (game.currentRound == 0) {
                ++game.currentRound;
            } else if (Game::Action::Check == game.prevAction) {
                ++game.currentRound;
            }
        } else if (Game::Action::Fold == action) {}
        game.updatePlayer();
        game.updateInfoSet(action);
    }


    void ActionStateBet::enter(Game &game, Game::Action action) {
        if (Game::Action::Raise1 == action) {
            game.setActions(std::vector<Game::Action>{Game::Action::Fold, Game::Action::Call, Game::Action::Reraise2});
        } else if (Game::Action::Reraise2 == action) {
            if (game.raiseNum >= Game::maxRaises) {
                game.setActions(std::vector<Game::Action>{Game::Action::Fold, Game::Action::Call});
            } else {
                game.setActions(std::vector<Game::Action>{Game::Action::Fold, Game::Action::Call, Game::Action::Reraise2});
            }
        }
    }

    void ActionStateBet::transition(Game &game, Game::Action action) {
        if (Game::Action::Call == action) {
            if (3 == game.currentRound) {
                game.setState(TerminalState::getInstance(), action);
            } else {
                game.setState(ChanceState::getInstance(), action);
            }
        } else if (Game::Action::Fold == action) {
            game.setState(TerminalState::getInstance(), action);
        } else if (Game::Action::Reraise2 == action) {
            if (Game::maxRaises < game.raiseNum) {
                throw std::logic_error("reraised more than allowed in actionbet");
            }
            game.setState(ActionStateBet::getInstance(), action);
        } else { throw std::logic_error("wrong action for actionbet"); }
    }

    GameState &ActionStateBet::getInstance() {
        static ActionStateBet singleton;
        return singleton;
    }


    void ActionStateBet::exit(Game &game, Game::Action action) {
        if (Game::Action::Call == action) {
            game.addMoney(1000);
            ++game.currentRound;
        } else if (Game::Action::Reraise2 == action) {
            game.addMoney(2000);
            ++game.raiseNum;
        } else if (Game::Action::Fold == action) {}
        game.updateInfoSet(action);
        game.updatePlayer();
    }


    void TerminalState::enter(Game &game, Game::Action action) {
        game.setType("terminal");
        //determine winner

        if (Game::Action::Fold == action) {
            game.winner = game.currentPlayer;
            return;
        }

        //std::copy(game.playableCards.begin()+4,game.playableCards.begin()+8,game.);
        std::array<int, 7> p0cards{game.playableCards[0], game.playableCards[1]};
        std::array<int, 7> p1cards{game.playableCards[2], game.playableCards[3]};

        std::copy(game.playableCards.begin() + 4, game.playableCards.begin() + 9, p0cards.begin() + 2);
        std::copy(game.playableCards.begin() + 4, game.playableCards.begin() + 9, p1cards.begin() + 2);

        game.winner = Utility::getWinner(p0cards.begin(), p1cards.begin());
    }

    void TerminalState::transition(Game &game, Game::Action action) {
        throw std::logic_error("cant transition from terminal unless?(reset?)");
    }

    GameState &TerminalState::getInstance() {
        static TerminalState singleton;
        return singleton;
    }

    void TerminalState::exit(Game &game, Game::Action action) {
        throw std::logic_error("shouldnt exit terminal state");
    }




}
