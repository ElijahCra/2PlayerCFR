//
// Created by elijah on 11/15/2023.
//

#ifndef CARDS_HPP
#define CARDS_HPP

#include "./HandAbstraction/hand_index.h"
#include <variant>
#include <vector>
#include <array>


class Cards {
explicit Cards(std::array<uint8_t,9> cards);

public:

    static hand_index_t plHandtoIndex();
    static std::vector<int> indexToCards();

    std::array<uint64_t,8> playerIndices{};
private:
    static hand_indexer_t riverIndexer;
    static inline bool init = false;
    static void indexerInit();
};



#endif //CARDS_HPP
