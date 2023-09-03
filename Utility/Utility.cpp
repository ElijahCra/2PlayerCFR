//
// Created by Elijah Crain on 8/29/23.
//

#include "Utility.hpp"
#include <iostream>

int Utility::HR[32487834];

bool Utility::initLookup() {
    printf("Testing the Two Plus Two 7-Card Evaluator\n");
    printf("-----------------------------------------\n\n");

    // Load the HandRanks.DAT file and map it into the HR array
    printf("Loading HandRanks.DAT file...\n");
    memset(HR, 0, sizeof(HR));
    FILE * fin = fopen("HandRanks.dat", "rb");
    if (!fin) {
        std::cout << "did not open properly \n";
        return false;
    }

    //size_t bytesread = fread(HR, sizeof(HR), 1, fin);	// get the HandRank Array
    fclose(fin);
    printf("complete.\n\n");
    return true;

}

int Utility::LookupHand(int* pCards)
{
    static int p = Utility::HR[53 + *pCards++];
    p = Utility::HR[p + *pCards++];
    p = Utility::HR[p + *pCards++];
    p = Utility::HR[p + *pCards++];
    p = Utility::HR[p + *pCards++];
    p = Utility::HR[p + *pCards++];
    return Utility::HR[p + *pCards++];
}