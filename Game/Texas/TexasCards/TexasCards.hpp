//
// Created by elijah on 11/15/2023.
//

#ifndef TEXASCARDS_HPP
#define TEXASCARDS_HPP

#include "../../Utility/HandAbstraction/hand_index.h"
#include <variant>
#include <vector>
#include <array>
#include <span>

namespace Texas {
class TexasCards {

public:
    explicit TexasCards();
    void initIndices(std::span<uint8_t, 9> cards);

    //static hand_index_t plHandtoIndex();
    //static std::vector<int> indexToCards();

    std::array<uint64_t,8> playerIndices{};
    static hand_indexer_t riverIndexer;
private:

    static inline bool init = false;
    static void indexerInit();
};
}
#endif //TEXASCARDS_HPP
