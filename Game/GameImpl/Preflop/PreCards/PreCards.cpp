//
// Created by elijah on 11/15/2023.
//

#include "PreCards.hpp"

namespace Preflop {
hand_indexer_t PreCards::flopIndexer;

PreCards::PreCards() {
  indexerInit();
}

void PreCards::initIndices(std::span<uint8_t, 9> cards) {
  hand_indexer_state_t hand1indeces;
  hand_indexer_state_t hand2indeces;

  hand_indexer_state_init(&flopIndexer, &hand1indeces);
  hand_indexer_state_init(&flopIndexer, &hand2indeces);

  const uint8_t cardsp0[]{cards[0], cards[1]};
  const uint8_t cardsp1[]{cards[2], cards[3]};
  const uint8_t cardsflop[]{cards[4], cards[5], cards[6], cards[7], cards[8]};


  playerIndices[0] = hand_index_next_round(&flopIndexer, cardsp0, &hand1indeces);
  playerIndices[2] = hand_index_next_round(&flopIndexer, cardsp1, &hand2indeces);

  playerIndices[1] = hand_index_next_round(&flopIndexer, cardsflop, &hand1indeces);
  playerIndices[3] = hand_index_next_round(&flopIndexer, cardsflop, &hand2indeces);


}

void PreCards::indexerInit() {
  if (!init) {
    constexpr uint8_t cardsperround[]{2, 5};
    hand_indexer_init(2, cardsperround, &flopIndexer);
    init = true;
  }
}
}

