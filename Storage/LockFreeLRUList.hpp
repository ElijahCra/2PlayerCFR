//
// Created by Elijah on 6/27/25.
//

#ifndef LOCKFREELRULIST_HPP
#define LOCKFREELRULIST_HPP

#include <atomic>
#include <memory>
#include <folly/synchronization/Hazptr.h>

namespace CFR {

template<typename T>
class LockFreeLRUList {
public:
    struct Node : folly::hazptr_obj_base<Node> {
        T value;
        std::atomic<Node*> next{nullptr};
        std::atomic<Node*> prev{nullptr};

        template<typename... Args>
        explicit Node(Args&&... args) : value(std::forward<Args>(args)...) {}
    };

    // Iterator type that wraps a hazptr_holder
    class iterator {
    public:
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        iterator() = default;
        explicit iterator(Node* node) : node_(node) {}

        reference operator*() { return node_->value; }
        pointer operator->() { return &node_->value; }

        Node* get_node() const { return node_; }

        bool operator==(const iterator& other) const {
            return node_ == other.node_;
        }

        bool operator!=(const iterator& other) const {
            return node_ != other.node_;
        }

    private:
        Node* node_ = nullptr;
    };

    LockFreeLRUList() {
        // Initialize sentinel nodes
        head_ = new Node();
        tail_ = new Node();
        head_->next.store(tail_, std::memory_order_relaxed);
        tail_->prev.store(head_, std::memory_order_relaxed);
    }

    ~LockFreeLRUList() {
        clear();
        delete head_;
        delete tail_;
    }

    // Add element to front of list
    template<typename... Args>
    iterator emplace_front(Args&&... args) {
        Node* new_node = new Node(std::forward<Args>(args)...);

        while (true) {
            folly::hazptr_holder<> h_head;
            folly::hazptr_holder<> h_first;

            Node* head = h_head.get_protected(head_);
            Node* first = h_first.get_protected(head->next);

            // Set up new node's pointers
            new_node->next.store(first, std::memory_order_relaxed);
            new_node->prev.store(head, std::memory_order_relaxed);

            // Try to update head->next
            Node* expected = first;
            if (head->next.compare_exchange_weak(expected, new_node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                // Success - now update first->prev
                first->prev.store(new_node, std::memory_order_release);
                return iterator(new_node);
            }
            // Retry on failure
        }
    }

    // Move node to front of list
    void move_to_front(Node* node) {
        if (!node || node == head_ || node == tail_) return;

        // First, unlink the node from its current position
        while (true) {
            folly::hazptr_holder<> h_prev;
            folly::hazptr_holder<> h_next;

            Node* prev = h_prev.get_protected(node->prev);
            Node* next = h_next.get_protected(node->next);

            // Check if node is already at front
            if (prev == head_) return;

            // Try to unlink
            Node* expected = node;
            if (prev->next.compare_exchange_weak(expected, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                next->prev.store(prev, std::memory_order_release);
                break;
            }
        }

        // Now insert at front
        while (true) {
            folly::hazptr_holder<> h_head;
            folly::hazptr_holder<> h_first;

            Node* head = h_head.get_protected(head_);
            Node* first = h_first.get_protected(head->next);

            node->next.store(first, std::memory_order_relaxed);
            node->prev.store(head, std::memory_order_relaxed);

            Node* expected = first;
            if (head->next.compare_exchange_weak(expected, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                first->prev.store(node, std::memory_order_release);
                break;
            }
        }
    }

    // Remove and return the last element
    Node* pop_back() {
        while (true) {
            folly::hazptr_holder<> h_tail;
            folly::hazptr_holder<> h_last;
            folly::hazptr_holder<> h_prev;

            Node* tail = h_tail.get_protected(tail_);
            Node* last = h_last.get_protected(tail->prev);

            // Check if list is empty
            if (last == head_) return nullptr;

            Node* prev = h_prev.get_protected(last->prev);

            // Try to unlink last node
            Node* expected = last;
            if (tail->prev.compare_exchange_weak(expected, prev,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                prev->next.store(tail, std::memory_order_release);

                // Clear the node's pointers
                last->next.store(nullptr, std::memory_order_relaxed);
                last->prev.store(nullptr, std::memory_order_relaxed);

                return last;
            }
        }
    }

    // Erase a specific node
    void erase(const iterator& it) {
        Node* node = it.get_node();
        if (!node || node == head_ || node == tail_) return;

        while (true) {
            folly::hazptr_holder<> h_prev;
            folly::hazptr_holder<> h_next;

            Node* prev = h_prev.get_protected(node->prev);
            Node* next = h_next.get_protected(node->next);

            // Try to unlink
            Node* expected = node;
            if (prev->next.compare_exchange_weak(expected, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                next->prev.store(prev, std::memory_order_release);

                // Clear the node's pointers and retire it
                node->next.store(nullptr, std::memory_order_relaxed);
                node->prev.store(nullptr, std::memory_order_relaxed);
                node->retire();
                break;
            }
        }
    }

    // Clear all elements
    void clear() {
        // Remove all nodes between head and tail
        while (true) {
            Node* node = pop_back();
            if (!node) break;
            node->retire();
        }
    }

    // Check if empty
    bool empty() const {
        folly::hazptr_holder<> h;
        Node* head = h.get_protected(head_);
        return head->next.load(std::memory_order_acquire) == tail_;
    }

private:
    Node* head_;  // Sentinel head node
    Node* tail_;  // Sentinel tail node
};

} // namespace CFR

#endif //LOCKFREELRULIST_HPP
