//
// Created by elijah on 9/28/2023.
//

#ifndef INC_2PLAYERCFR_G_H
#define INC_2PLAYERCFR_G_H

namespace Texas {
    class GameBase {
    public:
        /// constants
        static constexpr int PlayerNum = 2;

        static constexpr int DeckCardNum = 12;

        static constexpr int maxRaises = 2;
        /// constructors

        enum class Action : int {
            None = -1,
            Fold = 0,
            Check = 1,
            Call = 2,
            Raise = 3,
            Raise1 = 4,
            Raise2 = 5,
            Raise3 = 6,
            Raise5 = 7,
            Raise10 = 8,
            Reraise2 = 9,
            Reraise4 = 10,
            Reraise6 = 11,
            Reraise10 = 12,
            Reraise20 = 13,
            AllIn = 13,
        };

    };

} // Texas

#endif //INC_2PLAYERCFR_G_H
