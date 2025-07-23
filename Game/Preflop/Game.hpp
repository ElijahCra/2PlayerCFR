//
// Created by Elijah Crain on 8/27/23.
//

#ifndef INC_2PLAYERCFR_GAME_HPP
#define INC_2PLAYERCFR_GAME_HPP

#include <random>
#include <array>
#include "torch/torch.h"

#include "GameBase.hpp"
#include "GameState.hpp"
#include "PreCards/PreCards.hpp"

namespace Preflop
{
  class GameState;

  class Game : public GameBase
  {
    friend class ChanceState;
    friend class ActionStateNoBet;
    friend class ActionStateBet;
    friend class TerminalState;
    friend class PreflopTests_Game1_Test;

  public:
    ///Constructor
    explicit Game(std::mt19937 &engine); // try another rng? boost or xorshift
    
    ///Modifier
    void transition(Action action);
    void reInitialize();
    void updateAverageUtilitySum(float value);
    void updateAverageUtility(int i);
      void initCardTensors(std::span<uint8_t, 9> cards);


    /// Getters
    [[nodiscard]] inline GameState *getCurrentState() const noexcept{ return currentState; }
    [[nodiscard]] std::vector<Action> getActions() const noexcept;
    [[nodiscard]] float getUtility(int payoffPlayer) const;
    [[nodiscard]] std::string getInfoSet(int player) const noexcept;
    [[nodiscard]] std::string getType() const noexcept;
    [[nodiscard]] int getCurrentPlayer() const noexcept;
    [[nodiscard]] float getAverageUtility() const noexcept;
    [[nodiscard]] int getPlayableCards(int index) const noexcept;
    [[nodiscard]] std::array<unsigned char, 9>::iterator playableCardsBegin();
    [[nodiscard]] std::vector<torch::Tensor> getCardTensors(int player, int round) const noexcept;
    [[nodiscard]] torch::Tensor getBetTensor() const noexcept;
      [[nodiscard]] int getCurrentRound() const noexcept {return currentRound;}

    static constexpr int NUM_CARD_TYPES = 4;
    static constexpr int NUM_BET_FEATURES = 24;
    static constexpr int MAX_ACTIONS = 10;
    static constexpr int positions_per_round = 6;
    static constexpr int total_bet_positions = positions_per_round * NUM_CARD_TYPES;
 protected:
  /// Setters
  void setType(std::string type);
  void setState(GameState &newState, Action action);
  void setActions(std::vector <Action> actionVec);

  /// Modifiers
  void addMoney();
  void addMoney(float amount);
  void recordAction(Action action);

  void updateInfoSet();
  void updateInfoSet(Action action);
  void updateCurrentPlayer();

  /// utils
  static std::string actionToStr(Action action);

  /// Constants
  ///@brief how many unique deals are possible
  [[nodiscard]] constexpr int getChanceActionNum() {
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


  std::vector<float> bettingSequence;
  int currentBettingPosition = 0;
  std::array<uint8_t, 2*GameBase::PlayerNum+5> playableCards{};

  /// @brief number of raises + reraises played this round
  uint8_t raiseNum{};

  ///@brief rng engine, mersienne twister
  std::mt19937 &RNG;

  int winner = -1;

  int currentRound = 0;

  Action prevAction = Action::None;

  std::array<float,PlayerNum> playerStacks = {100.0f};

  PreCards cards;

  float averageUtility{};

  float averageUtilitySum{};

  std::array<std::vector<torch::Tensor>,2> m_cardTensors;
  torch::Tensor betTensor;

  std::string type = "chance";
  /// @brief the players private info set, contains their cards public cards and all actions played
  std::array <std::string, PlayerNum> infoSet{};

  /// @brief array of payoff, 1 per player final is the pot
  std::array<float, PlayerNum + 1> utilities{};

  ///@brief current gamestate i.e. preflop chance or preflopnobet
  GameState *currentState;

  ///@brief actions available at this point in the game
  std::vector <Action> availActions;
};
}


#endif //INC_2PLAYERCFR_GAME_HPP
