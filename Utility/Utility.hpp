//
// Created by Elijah Crain on 8/29/23.
//

#ifndef INC_2PLAYERCFR_UTILITY_HPP
#define INC_2PLAYERCFR_UTILITY_HPP


class Utility {

public:

    static bool initLookup();

    static int LookupHand(int* pCards);

    static int HR[32487834];

    Utility() = delete;

    static int getWinner(int *p0Cards, int *p1Cards);



private:




};


#endif //INC_2PLAYERCFR_UTILITY_HPP
