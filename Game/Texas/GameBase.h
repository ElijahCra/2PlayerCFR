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
            Check = 0,
            Fold = 1,
            Call = 2,
            Raise = 3,
            Reraise = 4
        };

    };

} // Texas

#endif //INC_2PLAYERCFR_G_H
