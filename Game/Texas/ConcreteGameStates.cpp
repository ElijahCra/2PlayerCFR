//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include "../../Utility/Utility.hpp"



void ChanceState::enter(Game *game, Action action) {
    switch (game->currentRound) {
        case Round::Preflop: {
            //deal cards
            if (DeckCardNum == 13) {
                for (int i = 0; i < DeckCardNum; ++i) {
                    game->deckCards[i] = 1 + i * 4;
                }
            } else {
                for (int i = 1; i <= DeckCardNum; ++i) {
                    game->deckCards[i - 1] = i;
                }
            }
            // shuffle cards
            std::shuffle(game->deckCards.begin(), game->deckCards.end(), game->RNG);

            //deal player cards
            for (int player = 0; player < PlayerNum; ++player) {
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
            game->setActions({Action::None});

            //put money into the pot for small and big blind
            game->addMoney();
            break;
        }
        case Round::Flop: {
            //deal flop cards to both players
            for (int player = 0; player < PlayerNum; ++player) {
                game->updateInfoSet(player,game->deckCards[2*PlayerNum]);
                game->updateInfoSet(player,game->deckCards[2*PlayerNum+1]);
                game->updateInfoSet(player,game->deckCards[2*PlayerNum+2]);
            }
            //set allowable actions to transfer from this node
            game->setActions({Action::None});
            break;
        }
        case Round::Turn: {
            //deal turn card to both players
            for (int player = 0; player < PlayerNum; ++player) {
                game->updateInfoSet(player,game->deckCards[2*PlayerNum+3]);
            }
            //set allowable actions to transfer from this node
            game->setActions({Action::None});
            break;
        }
        case Round::River: {
            //deal river card to both players
            for (int player = 0; player < PlayerNum; ++player) {
                game->updateInfoSet(player,game->deckCards[2*PlayerNum+4]);
            }
            //set allowable actions to transfer from this node
            game->setActions({Action::None});
            break;
        }
    }
}

void ChanceState::exit(Game *game, Action action) {
    if (Round::Preflop == game->currentRound) {
        game->currentPlayer = 0;
    } else {
        game->currentPlayer = 1;
    }
}

void ChanceState::transition(Game *game, Action action) {
        game->setState(ActionStateNoBet::getInstance(), Action::None);
}

GameState& ChanceState::getInstance() {
    static ChanceState singleton;
    return singleton;
}

void ActionStateNoBet::enter(Game *game, Action action) {
    if (Action::None == action) {
        game->setActions({Action::Fold,Action::Raise, Action::Call});
    } else if (Action::Call == action) {
        game->setActions({Action::Check, Action::Raise});
    }
    game->setType("action");
}

void ActionStateNoBet::transition(Game *game, Action action) {
    if (Action::Call == action) { //first action small-blind calls/limps
        game->setState(ActionStateNoBet::getInstance(), action);
    } else if (Action::Check == action) {
        if (Round::River == game->currentRound) {
            game->setState(TerminalState::getInstance(),action);
        } else {
            game->setState(ChanceState::getInstance(), action);
        }
    } else if (Action::Fold == action) { //first action small-blind folds
        game->setState(TerminalState::getInstance(), action);          // or second action bb checks -> post flop chance node
    } else if (Action::Raise == action) { //first action small blind raises
        game->setState(ActionStateBet::getInstance(), action);
    }
}

GameState& ActionStateNoBet::getInstance(){
    static ActionStateNoBet singleton;
    return singleton;
}

void ActionStateNoBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->addMoney(0.5);
    } else if (Action::Raise == action) {
        game->addMoney(1.5);
        ++game->raiseNum;
    } else if (Action::Check == action){
        ++game->currentRound;
    } else if (Action::Fold == action) {}
    game->updatePlayer();
    game->updateInfoSet(action);
}



void ActionStateBet::enter(Game *game, Action action) {
    if (Action::Raise == action) {
        game->setActions(std::vector<Action> {Action::Fold, Action::Call, Action::Reraise});
    } else if (Action::Reraise == action){
        if (game->raiseNum >= maxRaises) {
            game->setActions(std::vector<Action>{Action::Fold, Action::Call});
        } else {
            game->setActions(std::vector<Action>{Action::Fold, Action::Call,Action::Reraise});
        }
    }
}

void ActionStateBet::transition(Game *game, Action action) {
    if (Action::Call == action) {
        if (Round::River == game->currentRound) {
            game->setState(TerminalState::getInstance(), action);
        } else {
            game->setState(ChanceState::getInstance(), action);
        }
    } else if (Action::Fold == action) {
        game->setState(TerminalState::getInstance(), action);
    } else if (Action::Reraise == action) {
        if (maxRaises < game->raiseNum){
            throw std::logic_error("reraised more than allowed in actionbet");
        }
        game->setState(ActionStateBet::getInstance(), action);
    } else {throw std::logic_error("wrong action for actionbet");}
}

GameState& ActionStateBet::getInstance(){
    static ActionStateBet singleton;
    return singleton;
}



void ActionStateBet::exit(Game *game, Action action) {
    if (Action::Call == action) {
        game->addMoney(1.0);
        ++game->currentRound;
    } else if (Action::Reraise == action) {
        game->addMoney(2.0);
        ++game->raiseNum;
    } else if (Action::Fold == action){}
    game->updateInfoSet(action);
    game->updatePlayer();
}


void TerminalState::enter(Game *game, Action action) {
    game->setType("terminal");
    //determine winner

    if (Action::Fold == action){
        game->winner = game->currentPlayer;
        return;
    }

    //std::copy(game->deckCards.begin()+4,game->deckCards.begin()+8,game->);
    std::array<int,7> p0cards{game->deckCards[0], game->deckCards[1]};
    std::array<int,7> p1cards{game->deckCards[2], game->deckCards[3]};

    std::copy(game->deckCards.begin() + 4, game->deckCards.begin() + 9, p0cards.begin() + 2);
    std::copy(game->deckCards.begin() + 4, game->deckCards.begin() + 9, p1cards.begin() + 2);

    game->winner = Utility::getWinner(p0cards.begin(),p1cards.begin());
}

void TerminalState::transition(Game *game, Action action) {
    throw std::logic_error("cant transition from terminal unless?(reset?)");
}

GameState& TerminalState::getInstance()
{
    static TerminalState singleton;
    return singleton;
}


void TerminalState::exit(Game *game, Action action) {}


/*
void FlopChance::enter(Game *game, Action action) {}

void FlopChance::transition(Game *game, Action action) {

    game->setState(TerminalState::getInstance(), Action::None);
}

GameState& FlopChance::getInstance()
{
    static FlopChance singleton;
    return singleton;
}

void FlopChance::exit(Game *game, Action action) {}
*/