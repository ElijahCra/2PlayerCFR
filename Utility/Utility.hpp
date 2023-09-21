//
// Created by Elijah Crain on 8/29/23.
//

#ifndef INC_2PLAYERCFR_UTILITY_HPP
#define INC_2PLAYERCFR_UTILITY_HPP


class Utility {

public:

    Utility();

    static bool initLookup();

    static int LookupHand(int* pCards);

    static int HR[32487834];

    static const int getWinner(int *p0Cards, int *p1Cards);

    static void EnumerateAll7CardHands();

private:


};


#endif //INC_2PLAYERCFR_UTILITY_HPP
