//
// Created by elijah on 11/15/2023.
//

#include "Cards.hpp"

hand_indexer_t Cards::riverIndexer;

Cards::Cards() {
    indexerInit();
}

void Cards::initIndices(std::span<uint8_t, 9> cards) {
    hand_indexer_state_t hand1indeces;
    hand_indexer_state_t hand2indeces;

    hand_indexer_state_init(&riverIndexer,&hand1indeces);
    hand_indexer_state_init(&riverIndexer,&hand2indeces);

    playerIndices[0] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[0],cards[1]},&hand1indeces);
    playerIndices[4] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[2],cards[3]},&hand2indeces);

    playerIndices[1] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[4],cards[5],cards[6]},&hand1indeces);
    playerIndices[5] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[4],cards[5],cards[6]},&hand2indeces);

    playerIndices[2] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[7]},&hand1indeces);
    playerIndices[6] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[7]},&hand2indeces);

    playerIndices[3] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[8]},&hand1indeces);
    playerIndices[7] = hand_index_next_round(&riverIndexer,(uint8_t[]){cards[8]},&hand2indeces);
}

void Cards::indexerInit() {
    if (!init) {
        hand_indexer_init(4,(uint8_t[]){2,3,1,1},&riverIndexer);
        init = true;
    }
}


