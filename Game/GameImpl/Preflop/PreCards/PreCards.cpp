//
// Created by elijah on 11/15/2023.
//

#include "PreCards.hpp"
#include "hand_index.h"

namespace Preflop {
hand_indexer_t PreCards::riverIndexer;

PreCards::PreCards() {
  indexerInit();
}

void PreCards::initIndices(std::span<uint8_t, 9> cards) {
  hand_indexer_state_t hand1indeces;
  hand_indexer_state_t hand2indeces;

  hand_indexer_state_init(&riverIndexer, &hand1indeces);
  hand_indexer_state_init(&riverIndexer, &hand2indeces);

  const uint8_t cardsp0[]{cards[0], cards[1]};
  const uint8_t cardsp1[]{cards[2], cards[3]};
  const uint8_t cardsflop[]{cards[4], cards[5], cards[6]};
  const uint8_t cardsturn[]{cards[7]};
  const uint8_t cardsriver[]{cards[8]};

  playerIndices[0] = hand_index_next_round(&riverIndexer, cardsp0, &hand1indeces);
  playerIndices[4] = hand_index_next_round(&riverIndexer, cardsp1, &hand2indeces);

  playerIndices[1] = hand_index_next_round(&riverIndexer, cardsflop, &hand1indeces);
  playerIndices[5] = hand_index_next_round(&riverIndexer, cardsflop, &hand2indeces);

  playerIndices[2] = hand_index_next_round(&riverIndexer, cardsturn, &hand1indeces);
  playerIndices[6] = hand_index_next_round(&riverIndexer, cardsturn, &hand2indeces);

  playerIndices[3] = hand_index_next_round(&riverIndexer, cardsriver, &hand1indeces);
  playerIndices[7] = hand_index_next_round(&riverIndexer, cardsriver, &hand2indeces);
}

void PreCards::indexerInit() {
  if (!init) {
    constexpr uint8_t cardsperround[]{2, 3, 1, 1};
    hand_indexer_init(4, cardsperround, &riverIndexer);
    init = true;
  }
}
}

