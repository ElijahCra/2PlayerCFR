//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include "../../Utility/Utility.hpp"

namespace Texas {

    void ChanceState::enter(Game *game, Game::Action action) {
        game->setType("chance");
        switch (game->currentRound) {
            case 0: {
                //deal cards
                for (int i = 0; i < Game::DeckCardNum; ++i) {
                    game->deckCards[i] = i + 1;
                }
                // shuffle cards
                std::shuffle(game->deckCards.begin(), game->deckCards.end(), game->RNG);

                //deal player cards
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    int card1 = game->deckCards[2 * player];
                    int card2 = game->deckCards[1 + 2 * player];
                    if (card1 > card2) {
                        game->updateInfoSet(player, card1);
                        game->updateInfoSet(player, card2);
                    } else {
                        game->updateInfoSet(player, card2);
                        game->updateInfoSet(player, card1);
                    }
                }
                //set allowable actions to transfer from this node
                game->setActions({Game::Action::None});

                //put money into the pot for small and big blind
                game->addMoney();
                break;
            }
            case 1: {
                //deal flop cards to both players
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    game->updateInfoSet(player, game->deckCards[2 * Game::PlayerNum]);
                    game->updateInfoSet(player, game->deckCards[2 * Game::PlayerNum + 1]);
                    game->updateInfoSet(player, game->deckCards[2 * Game::PlayerNum + 2]);
                }
                //set allowable actions to transfer from this node
                game->setActions({Game::Action::None});
                break;
            }
            case 2: {
                //deal turn card to both players
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    game->updateInfoSet(player, game->deckCards[2 * Game::PlayerNum + 3]);
                }
                //set allowable actions to transfer from this node
                game->setActions({Game::Action::None});
                break;
            }
            case 3: {
                //deal river card to both players
                for (int player = 0; player < Game::PlayerNum; ++player) {
                    game->updateInfoSet(player, game->deckCards[2 * Game::PlayerNum + 4]);
                }
                //set allowable actions to transfer from this node
                game->setActions({Game::Action::None});
                break;
            }
        }
        game->raiseNum = 0;
    }

    void ChanceState::exit(Game *game, Game::Action action) {
        if (0 == game->currentRound) {
            game->currentPlayer = 0;
        } else {
            game->currentPlayer = 1;
        }
        game->prevAction = Game::Action::None;
    }

    void ChanceState::transition(Game *game, Game::Action action) {
        game->setState(ActionStateNoBet::getInstance(), Game::Action::None);
    }

    GameState &ChanceState::getInstance() {
        static ChanceState singleton;
        return singleton;
    }

    void ActionStateNoBet::enter(Game *game, Game::Action action) {
        if (0 == game->currentRound) {
            if (Game::Action::None == action) {
                game->setActions({Game::Action::Raise, Game::Action::Call, Game::Action::Fold});
            } else if (Game::Action::Call == action) {
                game->setActions({Game::Action::Raise, Game::Action::Check});
            }
        } else {
            game->setActions({Game::Action::Raise, Game::Action::Check});
        }
        game->setType("action");
    }

    void ActionStateNoBet::transition(Game *game, Game::Action action) {
        if (Game::Action::Call == action) { //first action small-blind calls/limps
            game->setState(ActionStateNoBet::getInstance(), action);
        } else if (Game::Action::Check == action) {
            if (3 == game->currentRound) {
                if (Game::Action::Check == game->prevAction) {
                    game->setState(TerminalState::getInstance(), action);
                } else {
                    game->setState(ActionStateNoBet::getInstance(), action);
                    game->prevAction = Game::Action::Check;
                }
            } else if (0 == game->currentRound){
                game->setState(ChanceState::getInstance(), action);
            } else {
                if (Game::Action::Check == game->prevAction) {
                    game->setState(ChanceState::getInstance(), action);
                } else {
                    game->setState(ActionStateNoBet::getInstance(), action);
                    game->prevAction = Game::Action::Check;
                }
            }
        } else if (Game::Action::Fold == action) { //first action small-blind folds
            game->setState(TerminalState::getInstance(),
                           action);          // or second action bb checks -> post flop chance node
        } else if (Game::Action::Raise == action) { //first action small blind raises
            game->setState(ActionStateBet::getInstance(), action);
        }
    }

    GameState &ActionStateNoBet::getInstance() {
        static ActionStateNoBet singleton;
        return singleton;
    }

    void ActionStateNoBet::exit(Game *game, Game::Action action) {
        if (Game::Action::Call == action) {
            game->addMoney(0.5);
        } else if (Game::Action::Raise == action) {
            game->addMoney(1.5);
            ++game->raiseNum;
        } else if (Game::Action::Check == action) {
            if (game->currentRound == 0) {
                ++game->currentRound;
            } else if (Game::Action::Check == game->prevAction) {
                ++game->currentRound;
            }
        } else if (Game::Action::Fold == action) {}
        game->updatePlayer();
        game->updateInfoSet(action);
    }


    void ActionStateBet::enter(Game *game, Game::Action action) {
        if (Game::Action::Raise == action) {
            game->setActions(std::vector<Game::Action>{Game::Action::Fold, Game::Action::Call, Game::Action::Reraise});
        } else if (Game::Action::Reraise == action) {
            if (game->raiseNum >= Game::maxRaises) {
                game->setActions(std::vector<Game::Action>{Game::Action::Fold, Game::Action::Call});
            } else {
                game->setActions(std::vector<Game::Action>{Game::Action::Fold, Game::Action::Call, Game::Action::Reraise});
            }
        }
    }

    void ActionStateBet::transition(Game *game, Game::Action action) {
        if (Game::Action::Call == action) {
            if (3 == game->currentRound) {
                game->setState(TerminalState::getInstance(), action);
            } else {
                game->setState(ChanceState::getInstance(), action);
            }
        } else if (Game::Action::Fold == action) {
            game->setState(TerminalState::getInstance(), action);
        } else if (Game::Action::Reraise == action) {
            if (Game::maxRaises < game->raiseNum) {
                throw std::logic_error("reraised more than allowed in actionbet");
            }
            game->setState(ActionStateBet::getInstance(), action);
        } else { throw std::logic_error("wrong action for actionbet"); }
    }

    GameState &ActionStateBet::getInstance() {
        static ActionStateBet singleton;
        return singleton;
    }


    void ActionStateBet::exit(Game *game, Game::Action action) {
        if (Game::Action::Call == action) {
            game->addMoney(1.0);
            ++game->currentRound;
        } else if (Game::Action::Reraise == action) {
            game->addMoney(2.0);
            ++game->raiseNum;
        } else if (Game::Action::Fold == action) {}
        game->updateInfoSet(action);
        game->updatePlayer();
    }


    void TerminalState::enter(Game *game, Game::Action action) {
        game->setType("terminal");
        //determine winner

        if (Game::Action::Fold == action) {
            game->winner = game->currentPlayer;
            return;
        }

        //std::copy(game->deckCards.begin()+4,game->deckCards.begin()+8,game->);
        std::array<int, 7> p0cards{game->deckCards[0], game->deckCards[1]};
        std::array<int, 7> p1cards{game->deckCards[2], game->deckCards[3]};

        std::copy(game->deckCards.begin() + 4, game->deckCards.begin() + 9, p0cards.begin() + 2);
        std::copy(game->deckCards.begin() + 4, game->deckCards.begin() + 9, p1cards.begin() + 2);

        game->winner = Utility::getWinner(p0cards.begin(), p1cards.begin());
    }

    void TerminalState::transition(Game *game, Game::Action action) {
        throw std::logic_error("cant transition from terminal unless?(reset?)");
    }

    GameState &TerminalState::getInstance() {
        static TerminalState singleton;
        return singleton;
    }

    void TerminalState::exit(Game *game, Game::Action action) {}

}
