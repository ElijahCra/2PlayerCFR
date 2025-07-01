//
// Created by elijah on 6/30/25.
//

#ifndef LRULIST_HPP
#define LRULIST_HPP
#include <list>
template<typename T>
class LRUList
{
public:
    using iterator = typename std::list<T>::iterator;
    void move_to_front(iterator it)
    {
        if (it == m_list.begin()) {
        return;
    }


    m_list.splice(m_list.begin(), m_list, it);
    };

    iterator emplace_front(std::string infoset, std::shared_ptr<CFR::Node>&& node)
    {
        return m_list.emplace_front(infoset, std::move(node));
    }
    
    iterator begin() { return m_list.begin(); }
    iterator end() { return m_list.end(); }
    
    std::list<T> m_list;

};

#endif //LRULIST_HPP
