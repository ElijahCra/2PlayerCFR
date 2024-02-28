//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_CONCRETEGAMESTATES_HPP
#define INC_2PLAYERCFR_CONCRETEGAMESTATES_HPP

#include "Game.hpp"

namespace Preflop {

class ChanceState : public GameState {
 public:
  void enter(Game &game, GameBase::Action action) override;

  void transition(Game &game, GameBase::Action action) override;

  void exit(Game &game, GameBase::Action action) override;

  static GameState &getInstance();

 private:
  ChanceState() = default;

  ChanceState(const ChanceState &copy);

  ChanceState &operator=(const ChanceState &copy);
};


class ActionStateNoBet : public GameState {
 public:
  void enter(Game &game, GameBase::Action action) override;

  void transition(Game &game, GameBase::Action action) override;

  void exit(Game &game, GameBase::Action action) override;

  static GameState &getInstance();

 private:
  ActionStateNoBet() = default;

  ActionStateNoBet(const ActionStateNoBet &copy);

  ActionStateNoBet &operator=(const ActionStateNoBet &copy);
};


class ActionStateBet : public GameState {
 public:
  void enter(Game &game, GameBase::Action action) override;

  void transition(Game &game, GameBase::Action action) override;

  void exit(Game &game, GameBase::Action action) override;

  static GameState &getInstance();

 private:
  ActionStateBet() = default;

  ActionStateBet(const ActionStateBet &copy);

  ActionStateBet &operator=(const ActionStateBet &copy);
};


class TerminalState : public GameState {
 public:
  void enter(Game &game, GameBase::Action action) override;

  void transition(Game &game, GameBase::Action action) override;

  void exit(Game &game, GameBase::Action action) override;

  static GameState &getInstance();

 private:
  TerminalState() = default;

  TerminalState(const TerminalState &copy);

  TerminalState &operator=(const TerminalState &copy);
};
}
#endif //INC_2PLAYERCFR_CONCRETEGAMESTATES_HPP
