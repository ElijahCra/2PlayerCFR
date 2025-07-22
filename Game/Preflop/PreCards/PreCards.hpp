//
// Created by elijah on 11/15/2023.
//

#ifndef CARDS_HPP
#define CARDS_HPP

#include "../../Utility/HandAbstraction/hand_index.h"
#include <variant>
#include <vector>
#include <array>
#include <span>

namespace Preflop {
class PreCards {

 public:
  explicit PreCards();
  void initIndices(std::span<uint8_t, 9> cards);

  std::array<uint64_t, 4> playerIndices{};
  static hand_indexer_t flopIndexer;
 private:

  static inline bool init = false;
  static void indexerInit();
};
} //Preflop

#endif //CARDS_HPP
