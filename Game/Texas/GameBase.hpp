//
// Created by elijah on 9/28/2023.
//

#ifndef INC_2PLAYERCFR_G_H
#define INC_2PLAYERCFR_G_H

#include <ranges>


namespace Texas {
    class GameBase {
    public:
        /// constants
        static constexpr int PlayerNum = 2;

        static constexpr int DeckCardNum = 52;

        static constexpr int maxRaises = 2;

        std::views::iota(1,52)
        static constexpr std::array<uint8_t,DeckCardNum> baseDeck = {};

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
