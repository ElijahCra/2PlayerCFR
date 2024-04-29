//
// Created by Elijah Crain on 3/25/24.
//

#ifndef CUSTOMEXCEPTIONS_H
#define CUSTOMEXCEPTIONS_H

#include <string>
class GameStageViolation : public std::exception {
private:
    std::string message;

public:
    GameStageViolation(std::string msg) : message(msg) {}
    std::string what () {
        return message;
    }
};

#endif //CUSTOMEXCEPTIONS_H
