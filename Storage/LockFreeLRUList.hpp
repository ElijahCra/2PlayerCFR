//
// Created by Assistant on 6/27/25.
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
    // A node must inherit from folly::hazptr_obj_base to be managed by hazard pointers. [cite: 16]
    struct Node : folly::hazptr_obj_base<Node> {
        T value;
        std::atomic<Node*> next{nullptr};
        std::atomic<Node*> prev{nullptr};

        template<typename... Args>
        explicit Node(Args&&... args) : value(std::forward<Args>(args)...) {}
    };

    // Iterator type that wraps a raw pointer
    class iterator {
    public:
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        iterator() = default;
        explicit iterator(Node* node) : node_(node) {}

        reference operator*() { return node_->value; }
        pointer operator->() { return &node_->value; }
        
        const T& operator*() const { return node_->value; }
        const T* operator->() const { return &node_->value; }

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
        folly::hazptr_holder<> h = folly::make_hazard_pointer(); // Re-use holder in the loop [cite: 32]

        while (true) {
            // Protect the current first node before modification.
            Node* first = h.protect(head_->next);

            // Set up new node's pointers
            new_node->next.store(first, std::memory_order_relaxed);
            new_node->prev.store(head_, std::memory_order_relaxed);

            // Try to update head->next
            Node* expected = first;
            if (head_->next.compare_exchange_weak(expected, new_node,
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

        // Unlink the node from its current position
        if (unlink(node)) {
            // Now insert at front
            insert_front(node);
        }
    }

    // Remove and return the last element
    Node* pop_back() {
        folly::hazptr_holder<> h_last = folly::make_hazard_pointer(); // Create holder outside the loop [cite: 32]

        while (true) {
            // Protect the 'last' node which we intend to remove.
            Node* last = h_last.protect(tail_->prev);

            // Check if list is empty
            if (last == head_) return nullptr;

            folly::hazptr_holder<> h_prev = folly::make_hazard_pointer();
            Node* prev = h_prev.protect(last->prev);

            if (!prev) continue; // Retry if previous node was removed

            // Try to unlink last node by updating its predecessor's 'next' pointer
            if (prev->next.compare_exchange_weak(last, tail_,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                tail_->prev.store(prev, std::memory_order_release);

                // Clear the node's pointers as it's now unlinked
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

        if (unlink(node)) {
            // For safe memory reclamation, retired objects are only deleted when no
            // thread holds a hazard pointer to them. [cite: 5, 38]
            node->retire();
        }
    }

    // Clear all elements
    void clear() {
        while (true) {
            Node* node = pop_back();
            if (!node) break;
            // The `retire()` function ensures deferred deletion. [cite: 20]
            node->retire();
        }
    }

    // Check if empty
    bool empty() const {
        return head_->next.load(std::memory_order_acquire) == tail_;
    }

private:
    Node* head_;  // Sentinel head node
    Node* tail_;  // Sentinel tail node

    // Helper to unlink a node, returns true on success
    bool unlink(Node* node) {
        folly::hazptr_array<2> h_neighbors = folly::make_hazard_pointer_array<2>(); // For protecting prev and next

        while (true) {
            Node* prev = h_neighbors[0].protect(node->prev);
            Node* next = h_neighbors[1].protect(node->next);

            if (!prev || !next) {
                // Node's links are changing, retry
                continue;
            }

            // Attempt to swing the 'next' pointer of the previous node to skip the current node.
            if (prev->next.compare_exchange_weak(node, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                // If successful, update the back-pointer of the next node.
                next->prev.store(prev, std::memory_order_release);
                return true;
            }

            // If the CAS failed, the state has changed, and the loop will retry.
        }
    }

    // Helper to insert a node at the front
    void insert_front(Node* node) {
        folly::hazptr_holder<> h = folly::make_hazard_pointer(); // Reuse holder [cite: 32]

        while (true) {
            // Protect the current head->next
            Node* first = h.protect(head_->next);

            node->next.store(first, std::memory_order_relaxed);
            node->prev.store(head_, std::memory_order_relaxed);

            // Attempt to swing head's next pointer to our new node
            if (head_->next.compare_exchange_weak(first, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                first->prev.store(node, std::memory_order_release);
                break;
            }
        }
    }
};

} // namespace CFR

#endif //LOCKFREELRULIST_HPP
