//
// Created by elijah on 9/28/2023.
//

#ifndef INC_2PLAYERCFR_G_H
#define INC_2PLAYERCFR_G_H
#include <cstdint>
#include <array>



namespace Texas {
    class GameBase {
    public:
        /// constants
        static constexpr uint8_t PlayerNum = 2;

        static constexpr uint8_t DeckCardNum = 52;

        static constexpr uint8_t maxRaises = 2;

        static constexpr std::array<uint8_t,DeckCardNum> rangeDeck = [] {
            std::array<uint8_t, DeckCardNum> deck{};

            for (uint8_t i = 0; i <DeckCardNum; ++i) {
                deck[i] = i+1;
            }

            return deck;
        }();

        static constexpr std::array<uint8_t, DeckCardNum> baseDeck = rangeDeck;

        enum class Action : int {
            None = -1,
            Fold,
            Check,
            Call,
            Raise1,
            Raise2,
            Raise3,
            Raise5,
            Raise10,
            Reraise2,
            Reraise4,
            Reraise6,
            Reraise10,
            Reraise20,
            AllIn
        };
    };
} // Texas

#endif //INC_2PLAYERCFR_G_H
