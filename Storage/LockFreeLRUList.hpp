//
// Created by elijah on 6/26/25.
//

#ifndef LOCKFREELRULIST_HPP
#define LOCKFREELRULIST_HPP

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#include <folly/synchronization/Hazptr.h>
#include <iterator>

template <typename T>
class LockFreeLRUList {
public:
    struct Node;
    class iterator;

private:
    // Low-order bit used as a mark for logical deletion.
    static constexpr uintptr_t MARK_BIT = 1;

    struct alignas(folly::hardware_destructive_interference_size) SentinelNode {
        std::atomic<Node*> next;
        std::atomic<Node*> prev;
    };

    alignas(folly::hardware_destructive_interference_size) SentinelNode head_;
    alignas(folly::hardware_destructive_interference_size) SentinelNode tail_;

    std::atomic<size_t> size_{0};

    static inline Node* get_ptr(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~MARK_BIT);
    }

    static inline bool is_marked(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & MARK_BIT) != 0;
    }

    static inline Node* mark(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | MARK_BIT);
    }

    // Physically unlinks a logically deleted (marked) node.
    void help_unlink(folly::hazptr_holder<>& h, Node* prev, Node* node) {
        Node* next_unmarked = get_ptr(node->next.load(std::memory_order_acquire));
        Node* protected_next = h.protect(next_unmarked);

        if (next_unmarked != protected_next) return; // Next node was reclaimed

        prev->next.compare_exchange_strong(node, protected_next, std::memory_order_release);
        if (protected_next) {
            protected_next->prev.compare_exchange_strong(node, prev, std::memory_order_release);
        }
    }


public:
    struct Node : public folly::hazptr_obj_base<Node> {
        T value;
        std::atomic<Node*> next;
        std::atomic<Node*> prev;

        template<typename... Args>
        explicit Node(Args&&... args) : value(std::forward<Args>(args)...), next(nullptr), prev(nullptr) {}
    };

    LockFreeLRUList() {
        head_.next.store(reinterpret_cast<Node*>(&tail_), std::memory_order_relaxed);
        tail_.prev.store(reinterpret_cast<Node*>(&head_), std::memory_order_relaxed);
    }

    ~LockFreeLRUList() {
        clear();
    }

    // Disallow copy and move semantics.
    LockFreeLRUList(const LockFreeLRUList&) = delete;
    LockFreeLRUList& operator=(const LockFreeLRUList&) = delete;

    // Logically and then physically unlinks a node from the list.
    bool unlink(Node* node) {
        folly::hazptr_holder h;
        while (true) {
            Node* next_node = h.protect(node->next);
            if (is_marked(next_node)) {
                return false; // Already unlinked by another thread
            }
            // Mark the node's next pointer to logically delete it.
            if (node->next.compare_exchange_weak(next_node, mark(next_node), std::memory_order_acq_rel)) {
                // Mark succeeded, now physically unlink.
                Node* prev = get_ptr(h.protect(node->prev));
                if(prev) {
                    help_unlink(h, prev, node);
                }
                return true;
            }
        }
    }

    // Pushes a node to the front of the list.
    void push_front(Node* node) {
        folly::hazptr_holder h;
        while (true) {
            Node* old_first = h.protect(head_.next);
            node->next.store(old_first, std::memory_order_relaxed);
            node->prev.store(reinterpret_cast<Node*>(&head_), std::memory_order_relaxed);

            // Atomically swing the head's next pointer to our new node.
            if (head_.next.compare_exchange_weak(old_first, node, std::memory_order_release)) {
                if (get_ptr(old_first) != reinterpret_cast<Node*>(&tail_)) {
                    old_first->prev.store(node, std::memory_order_release);
                }
                size_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    // Moves an existing node to the front of the list.
    void move_to_front(Node* node) {
        if (get_ptr(head_.next.load(std::memory_order_acquire)) == node) {
            return; // Already at the front
        }
        if (unlink(node)) {
            // After unlinking, the size was decremented. We add it back here.
            size_.fetch_sub(1, std::memory_order_relaxed);
            push_front(node);
        }
    }

    Node* pop_back() {
       folly::hazptr_holder h;
       while (true) {
           Node* last = get_ptr(h.protect(tail_.prev));
           if (last == reinterpret_cast<Node*>(&head_)) {
               return nullptr; // List is empty
           }
           if (unlink(last)) {
               size_.fetch_sub(1, std::memory_order_relaxed);
               return last;
           }
       }
    }

    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator() : node_(nullptr) {}

        explicit iterator(Node* node) : node_(node) {}

        reference operator*() const { return node_->value; }
        pointer operator->() const { return &node_->value; }

        iterator& operator++() {
            if (node_) {
                folly::hazptr_holder h;
                Node* current = node_;
                do {
                    current = get_ptr(h.protect(current->next));
                } while (current && current != reinterpret_cast<Node*>(&(((LockFreeLRUList*)0)->tail_)) && is_marked(current->next.load()));

                if (current == reinterpret_cast<Node*>(&(((LockFreeLRUList*)0)->tail_))) {
                    node_ = nullptr;
                } else {
                    node_ = current;
                }
            }
            return *this;
        }

        bool operator==(const iterator& other) const { return node_ == other.node_; }
        bool operator!=(const iterator& other) const { return node_ != other.node_; }

        Node* get_node() const { return node_; }

    private:
        Node* node_;
    };

    size_t size() const {
        return size_.load(std::memory_order_relaxed);
    }

    bool empty() const {
        return get_ptr(head_.next.load()) == reinterpret_cast<Node*>(&tail_);
    }

    iterator begin() {
        folly::hazptr_holder h;
        Node* first = get_ptr(h.protect(head_.next));
        if (first == reinterpret_cast<Node*>(&tail_)) {
            return iterator(nullptr);
        }
        return iterator(first);
    }

    iterator end() {
        return iterator(nullptr);
    }

    template<typename... Args>
    iterator emplace_front(Args&&... args) {
        Node* node = new Node(std::forward<Args>(args)...);
        push_front(node);
        return iterator(node);
    }

    iterator erase(iterator it) {
        if (it.get_node() == nullptr) return end();
        Node* node_to_erase = it.get_node();
        iterator next_it = it;
        ++next_it;

        if (unlink(node_to_erase)) {
            size_.fetch_sub(1, std::memory_order_relaxed);
            node_to_erase->retire();
        }
        return next_it;
    }

    T& back() {
        folly::hazptr_holder h;
        Node* last = get_ptr(h.protect(tail_.prev));
        if (last == reinterpret_cast<Node*>(&head_)) {
            throw std::runtime_error("back() called on empty list");
        }
        return last->value;
    }

    void clear() {
        while (pop_back() != nullptr) {
            // pop_back handles retirement
        }
    }
};

#endif //LOCKFREELRULIST_HPP
