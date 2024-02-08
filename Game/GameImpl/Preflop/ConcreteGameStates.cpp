//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include "../../Utility/Utility.hpp"

namespace Preflop {

    void ChanceState::enter(Game *game, Game::Action action) {
        // shuffle cards
        std::shuffle(game->deckCards.begin(), game->deckCards.end(), game->RNG);

        //deal player cards
        for (int player = 0; player < Game::PlayerNum; ++player) {
            const int card1 = game->deckCards[2 * player];
            const int card2 = game->deckCards[1 + 2 * player];
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
    }

    void ChanceState::transition(Game *game, Game::Action action) {
        if (Game::Action::None == action) {
            game->setState(ActionStateNoBet::getInstance(), Game::Action::None);
        } else { throw std::logic_error("wrong action for preflopchance"); }
        //std::cout << "transitioned from preflop chance \n";
    }

    GameState &ChanceState::getInstance() {
        static ChanceState singleton;
        return singleton;
    }

    void ChanceState::exit(Game *game, Game::Action action) {
        game->setActions(std::vector<Game::Action>{Game::Action::None});
        game->currentPlayer = 0; //dealer (p0) posts sb and plays first preflop

    }


    void ActionStateNoBet::enter(Game *game, Game::Action action) {
        if (Game::Action::None == action) {
            std::vector<Game::Action> availActions{Game::Action::Fold, Game::Action::Raise, Game::Action::Call};
            game->setActions(availActions);
        } else if (Game::Action::Call == action) {
            std::vector<Game::Action> availActions{Game::Action::Check, Game::Action::Raise};
            game->setActions(availActions);
        }
        game->setType("action");
    }

    void ActionStateNoBet::transition(Game *game, Game::Action action) {
        if (Game::Action::Call == action) { //first action small-blind calls/limps
            game->setState(ActionStateNoBet::getInstance(), action);
        } else if (Game::Action::Fold == action or Game::Action::Check == action) { //first action small-blind folds
            game->setState(TerminalState::getInstance(),
                           action);          // or second action bb checks -> post flop chance node
        } else if (Game::Action::Raise == action) { //first action small blind raises
            game->setState(ActionStateBet::getInstance(), action);
        } else { throw std::logic_error("wrong action for preflopnobet"); }
        //std::cout << "transitioned from preflop no bet \n";
    }

    GameState &ActionStateNoBet::getInstance() {
        static ActionStateNoBet singleton;
        return singleton;
    }


    void ActionStateNoBet::exit(Game *game, Game::Action action) {
        if (Game::Action::Call == action) {
            game->addMoney(500);

        } else if (Game::Action::Raise == action) {
            game->addMoney(1500);
            ++game->raiseNum;
        } else if (Game::Action::Check == action) {}

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
        if (Game::Action::Call == action or Game::Action::Fold == action) { //previous player raises this player calls -> FC
            game->setState(TerminalState::getInstance(), action);    //or previous player raises this player folds -> FC
        } else if (Game::Action::Reraise ==
                   action) { //previous player raises we reraise -> PFB player has to decide then -> FC
            if (Game::maxRaises < game->raiseNum) {
                throw std::logic_error("reraised more than allowed in preflopactionbet");
            }
            game->setState(ActionStateBet::getInstance(), action);
        } else { throw std::logic_error("wrong action for preflopbet"); }

        //std::cout << "transitioned from preflop bet \n";
    }

    GameState &ActionStateBet::getInstance() {
        static ActionStateBet singleton;
        return singleton;
    }


    void ActionStateBet::exit(Game *game, Game::Action action) {
        if (Game::Action::Call == action) {
            game->addMoney(1000);
        } else if (Game::Action::Reraise == action) {
            game->addMoney(2000);
            ++game->raiseNum;
        } else if (Game::Action::Fold == action) {}
        game->updateInfoSet(action);
        game->updatePlayer();
    }


    void TerminalState::enter(Game *game, Game::Action action) {
        game->setType("terminal");
        game->setActions(std::vector<Game::Action>{Game::Action::None});

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