//
// Created by Elijah Crain on 8/27/23.
//

#include "ConcreteGameStates.hpp"
#include <iostream>
#include <algorithm>
#include "../../Utility/Utility.hpp"

namespace Preflop {

void ChanceState::enter(Game &game, Game::Action action) {
  game.setType("chance");
  game.raiseNum = 0;
  game.setActions({Game::Action::None});
  game.updateInfoSet();
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
  using enum Preflop::GameBase::Action;
  if (0 == game.currentRound) {
    if (None == action) {
      game.setActions({Raise1, Call, Fold});
    } else if (Call == action) {
      game.setActions({Raise1, Check});
    }
  } else {
    game.setActions({Raise1, Check});
  }
  game.setType("action");
}

void ActionStateNoBet::transition(Game &game, Game::Action action) {
  using enum Preflop::GameBase::Action;
  if (Call == action) { //first action small-blind calls/limps
    game.setState(ActionStateNoBet::getInstance(), action);
  } else if (Check == action) {
    if (1 == game.currentRound) {
      if (Check == game.prevAction) {
        game.setState(TerminalState::getInstance(), action);
      } else {
        game.setState(ActionStateNoBet::getInstance(), action);
        game.prevAction = Check;
      }
    } else if (0 == game.currentRound){
      game.setState(ChanceState::getInstance(), action);
    } else {
      if (Check == game.prevAction) {
        game.setState(ChanceState::getInstance(), action);
      } else {
        game.setState(ActionStateNoBet::getInstance(), action);
        game.prevAction = Check;
      }
    }
  } else if (Fold == action) { //first action small-blind folds
    game.setState(TerminalState::getInstance(),
                  action);          // or second action bb checks -> post flop chance node
  } else if (Raise1 == action) { //first action small blind raises
    game.setState(ActionStateBet::getInstance(), action);
  }
}

GameState &ActionStateNoBet::getInstance() {
  static ActionStateNoBet singleton;
  return singleton;
}

void ActionStateNoBet::exit(Game &game, Game::Action action) {
  using enum Preflop::GameBase::Action;
  if (Call == action) {
    game.addMoney(500);
  } else if (Raise1 == action) {
    game.addMoney(1500);
    ++game.raiseNum;
  } else if (Check == action) {
    if (game.currentRound == 0) {
      ++game.currentRound;
    } else if (Check == game.prevAction) {
      ++game.currentRound;
    }
  }
  game.updateCurrentPlayer();
  game.updateInfoSet(action);
}


void ActionStateBet::enter(Game &game, Game::Action action) {
  using enum Preflop::GameBase::Action;
  if (Raise1 == action) {
    game.setActions(std::vector<Game::Action>{Fold, Call, Reraise2});
  } else if (Reraise2 == action) {
    if (game.raiseNum >= Game::maxRaises) {
      game.setActions(std::vector<Game::Action>{Fold, Call});
    } else {
      game.setActions(std::vector<Game::Action>{Fold, Call, Reraise2});
    }
  }
}

void ActionStateBet::transition(Game &game, Game::Action action) {
  using enum Preflop::GameBase::Action;
  if (Call == action) {
    if (1 == game.currentRound) {
      game.setState(TerminalState::getInstance(), action);
    } else {
      game.setState(ChanceState::getInstance(), action);
    }
  } else if (Fold == action) {
    game.setState(TerminalState::getInstance(), action);
  } else if (Reraise2 == action) {
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
  using enum Preflop::GameBase::Action;
  if (Call == action) {
    game.addMoney(1000);
    ++game.currentRound;
  } else if (Reraise2 == action) {
    game.addMoney(2000);
    ++game.raiseNum;
  }
  game.updateInfoSet(action);
  game.updateCurrentPlayer();
}


void TerminalState::enter(Game &game, Game::Action action) {
  game.setType("terminal");
  //determine winner

  if (Game::Action::Fold == action) {
    game.winner = game.currentPlayer;
    return;
  }

  std::array<int, 7> p0cards{game.getPlayableCards(0), game.getPlayableCards(1)};
  std::array<int, 7> p1cards{game.getPlayableCards(2), game.getPlayableCards(3)};

  std::copy(game.playableCardsBegin() + 4, game.playableCardsBegin() + 9, p0cards.begin() + 2);
  std::copy(game.playableCardsBegin() + 4, game.playableCardsBegin() + 9, p1cards.begin() + 2);

  for (int i=0; i<7;++i){
    p0cards[i] += 1;
    p1cards[i] += 1;
  }

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