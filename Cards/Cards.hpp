//
// Created by elijah on 11/15/2023.
//

#ifndef CARDS_HPP
#define CARDS_HPP

#include "./HandAbstraction/hand_index.h"
#include <variant>
#include <vector>


class Cards {
Cards(int card1, int card2);
Cards(int card1, int card2, int card3);

public:
    static hand_index_t plHandtoIndex();
    static std::vector<int> indexToCards();

    std::array<std::variant<int,hand_index_t>,5> publicHand;
    std::vector<std::variant<int,hand_index_t>> Hand;
};



#endif //CARDS_HPP