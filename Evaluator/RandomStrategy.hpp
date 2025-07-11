//
// Created by elijah on 7/11/25.
//

#ifndef RANDOMSTRATEGY_HPP
#define RANDOMSTRATEGY_HPP
#include "../Storage/NodeStorage.hpp"
#include "../CFR/Node.hpp"
#include <regex>
#include <unordered_map>

template <typename GameType>
class RandomStrategy : public CFR::NodeStorage
{
public:
    std::shared_ptr<CFR::Node> getNode(const std::string& infoSet) override
    {
       return nullptr;
    }
    
    void putNode(const std::string& infoSet, std::shared_ptr<CFR::Node> node) override {}

    bool hasNode(const std::string& infoSet) const override {return true;}

    void removeNode(const std::string& infoSet) override {}

    [[nodiscard]] size_t size() const override {return 0;}
    void clear() {};

};

#endif //RANDOMSTRATEGY_HPP
