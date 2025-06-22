//
// Created by Elijah Crain on 3/25/24.
//

#ifndef CUSTOMEXCEPTIONS_H
#define CUSTOMEXCEPTIONS_H

#include <string>
#include <utility>
class GameStageViolation : public std::exception {
private:
    std::string message;

public:
    explicit GameStageViolation(std::string msg) : message(std::move(msg)) {}
    [[nodiscard]] const char* what () const noexcept override{
        return message.c_str();
    }
};

#endif //CUSTOMEXCEPTIONS_H
