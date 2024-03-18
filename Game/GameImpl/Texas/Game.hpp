//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_TEXAS_GAME_HPP
#define INC_TEXAS_GAME_HPP

#include <random>
#include <array>
#include "GameBase.hpp"
#include "GameState.hpp"
#include "TexasCards.hpp"

namespace Texas {
class GameState;

class Game : public GameBase {
  friend class ChanceState;
  friend class ActionStateNoBet;
  friend class ActionStateBet;
  friend class TerminalState;
  friend class TexasTests_Game1_Test;

 public:
  ///Constructor
  explicit Game(std::mt19937 &engine); //try another rng? boost or xorshift

  ///Modifier
  void transition(Action action);
  void reInitialize();



  /// Getters
  [[nodiscard]] inline GameState *getCurrentState() const noexcept{ return currentState; }
  [[nodiscard]] std::vector<Action> getActions() const noexcept;
  [[nodiscard]] float getUtility(int payoffPlayer) const;
  [[nodiscard]] std::string getInfoSet(int player) const noexcept;
  [[nodiscard]] std::string getType() const noexcept;
  [[nodiscard]] int getCurrentPlayer() const noexcept;
  [[nodiscard]] float getAverageUtility() const noexcept;

  ///@brief deck of cards
  std::array<uint8_t, 2*PlayerNum+5> playableCards{};
 protected:

  /// Setters
  void setType(std::string type);
  void setState(GameState &newState, Action action);
  void setActions(std::vector <Action> actionVec);

  /// Modifiers
  void addMoney();
  void addMoney(float amount);

  void updateInfoSet();
  void updateInfoSet(Action action);

  void updateAverageUtilitySum(float value);
  void updateAverageUtility(int i);
  void updatePlayer();


  /// members

  /// utils
  static std::string actionToStr(Action action);

  /// Constants
  ///@brief how many unique deals are possible
  [[nodiscard]] constexpr int getChanceActionNum() const{
    if (0 == currentRound) {
      //(cardNum choose 2) * (cardNum-2 choose 2)
      return DeckCardNum * (DeckCardNum - 1) * (DeckCardNum - 2) * (DeckCardNum - 3);
    } else if (1 == currentRound) {
      return (DeckCardNum - 4) * (DeckCardNum - 5) * (DeckCardNum - 6);
    } else {
      return DeckCardNum - (currentRound + 5);
    }
  }
 private:
  std::string type = "chance";

  Action prevAction = Action::None;

  std::array<float,PlayerNum> playerStacks = {100.0f};

  int winner = -1;

  int currentRound = 0;

  /// @brief the players private info set, contains their cards public cards and all actions played
  std::array <std::string, PlayerNum> infoSet{};

  /// @brief array of payoff, 1 per player final is the pot
  std::array<float, PlayerNum + 1> utilities{};

  ///@brief current gamestate i.e. preflop chance or preflopnobet
  GameState *currentState;

  ///@brief actions available at this point in the game
  std::vector <Action> availActions;

  TexasCards cards;

  /// @brief acting player
  int currentPlayer;

  float averageUtility{};

  float averageUtilitySum{};

  /// @brief number of raises + reraises played this round
  uint8_t raiseNum{};

  ///@brief rng engine, mersienne twister
  std::mt19937 &RNG;


};
}

#endif //INC_TEXAS_GAME_HPP
