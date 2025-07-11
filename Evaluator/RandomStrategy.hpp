//
// Created by elijah on 7/11/25.
//

#ifndef RANDOMSTRATEGY_HPP
#define RANDOMSTRATEGY_HPP
#include "../Storage/NodeStorage.hpp"

template <typename GameType>
class RandomStrategy : CFR::NodeStorage
{
    std::shared_ptr<CFR::Node> getNode(const std::string& infoSet)
    {
        
    }
    void putNode(const std::string& infoSet, std::shared_ptr<CFR::Node> node) {}

    bool hasNode(const std::string& infoSet) const {return true;}

    void removeNode(const std::string& infoSet) {}

    [[nodiscard]]  size_t size() const {}
};

#endif //RANDOMSTRATEGY_HPP
