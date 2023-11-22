//
// Created by Elijah Crain on 8/29/23.
//

#ifndef INC_2PLAYERCFR_UTILITY_HPP
#define INC_2PLAYERCFR_UTILITY_HPP

#include <array>
#include <vector>
#include <sys/types.h>

#include "../Cards/HandAbstraction/hand_index.h"


class Utility {

public:

    Utility();



    static int LookupHand(int* pCards);



    static int getWinner(int *p0Cards, int *p1Cards);

    static void EnumerateAll7CardHands();

    static int LookupSingleHands();

    static hand_indexer_t preflop_indexer;
    static hand_indexer_t flop_indexer;
    static hand_indexer_t turn_indexer;
    static hand_indexer_t river_indexer;

    static uint64_t cardsToIndex(int round, std::vector<uint32_t> cards);
private:
    static bool initLookup();
    static int HR[32487834];
    static bool initialized;
};


#endif //INC_2PLAYERCFR_UTILITY_HPP
