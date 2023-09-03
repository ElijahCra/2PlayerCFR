#include <iostream>

#include "Game/Game.hpp"
#include <random>
#include "Utility/Utility.hpp"

int main() {

    //uint32_t seed = std::random_device()();

    //std::mt19937 RNG(seed);

    //Game game = Game(RNG);

    //game.transition(Action::None);

    //Utility::initLookup();

    //int* cards = new int[7] {1,2,3,4,5,6,7};
    //Utility::LookupHand(cards);
    const int sizer = 32487834;

    int* holder = new int[sizer];
    printf("Testing the Two Plus Two 7-Card Evaluator\n");
    printf("-----------------------------------------\n\n");

    // Load the HandRanks.DAT file and map it into the HR array
    printf("Loading HandRanks.DAT file...");
    memset(holder, 0, sizeof(sizer));
    FILE * fin = fopen("./Utility/HandRanks.dat", "rb");
    if (!fin) {std::cout << "fail"; return 1;}

    fclose(fin);
    printf("complete.\n\n");



    //std::cout << Utility::LookupHand(cards) << std::endl;
}
