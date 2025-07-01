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
    const char* what () const noexcept override {
        return message.c_str();
    }
};

#endif //CUSTOMEXCEPTIONS_H
