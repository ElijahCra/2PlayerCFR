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

template <typename T>
class LockFreeLRUList {
public:
    struct Node;

private:
    // Low-order bit used as a mark for logical deletion.
    static constexpr uintptr_t MARK_BIT = 1;

    // Aligned sentinel nodes to simplify boundary conditions.
    alignas(folly::hardware_destructive_interference_size) Node head_;
    alignas(folly::hardware_destructive_interference_size) Node tail_;

    // Helper functions for marked pointer manipulation
    static inline Node* get_ptr(uintptr_t p) {
        return reinterpret_cast<Node*>(p & ~MARK_BIT);
    }

    static inline bool is_marked(uintptr_t p) {
        return (p & MARK_BIT)!= 0;
    }

    static inline uintptr_t mark(Node* p) {
        return reinterpret_cast<uintptr_t>(p) | MARK_BIT;
    }

    // Physically unlinks a logically deleted (marked) node.
    // This is the "helping" function.
    void finish_unlink(Node* node) {
        folly::hazptr_holder h_prev, h_next;

        // Find the first non-marked predecessor.
        Node* prev = get_ptr(node->prev.load());
        while (true) {
            h_prev.protect(prev);
            uintptr_t prev_next_val = prev->next.load(std::memory_order_relaxed);
            if (get_ptr(prev_next_val)!= node || is_marked(prev_next_val)) {
                // Predecessor's link is already changed or node is unlinked.
                break;
            }
            if (is_marked(prev->next.load())) {
                 // Predecessor is also being deleted, need to help it first.
                 // In a full implementation, one would call finish_unlink(prev) here.
                 // For simplicity, we'll just search for an earlier valid predecessor.
                 prev = get_ptr(prev->prev.load());
                 continue;
            }
            break;
        }

        // Swing predecessor's next pointer to bypass the node.
        Node* next_node = get_ptr(node->next.load());
        uintptr_t expected_node_ptr = reinterpret_cast<uintptr_t>(node);
        uintptr_t next_node_ptr = reinterpret_cast<uintptr_t>(next_node);

        // This CAS is the primary step of physical unlinking from the forward chain.
        prev->next.compare_exchange_strong(expected_node_ptr, next_node_ptr, std::memory_order_release);

        // Swing successor's prev pointer to bypass the node.
        h_next.protect(next_node);
        if (get_ptr(next_node->prev.load()) == node) {
            next_node->prev.compare_exchange_strong(
                expected_node_ptr,
                reinterpret_cast<uintptr_t>(prev),
                std::memory_order_release);
        }
    }


public:
    // Node structure inherits from hazptr_obj_base for safe reclamation.
    struct Node : public folly::hazptr_obj_base<Node> {
        T value;
        std::atomic<uintptr_t> next;
        std::atomic<uintptr_t> prev;

        Node() : value(), next(0), prev(0) {}
        explicit Node(T val) : value(std::move(val)), next(0), prev(0) {}
    };

    LockFreeLRUList() {
        head_.next.store(reinterpret_cast<uintptr_t>(&tail_), std::memory_order_relaxed);
        tail_.prev.store(reinterpret_cast<uintptr_t>(&head_), std::memory_order_relaxed);
    }

    ~LockFreeLRUList() {
        // In a real application, one would need to drain the list.
        // For this example, we assume the list is empty upon destruction.
    }

    // Disallow copy and move semantics.
    LockFreeLRUList(const LockFreeLRUList&) = delete;
    LockFreeLRUList& operator=(const LockFreeLRUList&) = delete;

    // Moves an existing node to the front of the list.
    // This is the core operation for an LRU cache hit.
    void move_to_front(Node* node) {
        if (get_ptr(head_.next.load()) == node) {
            // Already at the front, no action needed.
            return;
        }
        if (unlink(node)) {
            push_front(node);
        }
    }

    // Logically and physically unlinks a node from the list.
    bool unlink(Node* node) {
        uintptr_t next_ptr = node->next.load(std::memory_order_relaxed);

        // Loop to mark the node's next pointer, indicating logical deletion.
        while (!is_marked(next_ptr)) {
            if (node->next.compare_exchange_weak(next_ptr, mark(get_ptr(next_ptr)),
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                // Mark succeeded. Now perform physical cleanup.
                finish_unlink(node);
                return true;
            }
            // CAS failed, next_ptr was updated with the current value.
        }
        // Node was already marked by another thread.
        return false;
    }

    // Pushes a node to the front of the list (after the head sentinel).
    void push_front(Node* node) {
        folly::hazptr_holder h_old_first;

        while (true) {
            uintptr_t head_next_val = head_.next.load(std::memory_order_acquire);
            Node* old_first = get_ptr(head_next_val);
            h_old_first.protect(old_first);

            // Re-validate after protecting.
            if (head_.next.load(std::memory_order_relaxed)!= head_next_val) {
                continue;
            }

            node->next.store(head_next_val, std::memory_order_relaxed);
            node->prev.store(reinterpret_cast<uintptr_t>(&head_), std::memory_order_relaxed);

            uintptr_t new_node_ptr = reinterpret_cast<uintptr_t>(node);

            // Atomically swing the head's next pointer to our new node.
            if (head_.next.compare_exchange_weak(head_next_val, new_node_ptr,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
                // Successfully inserted. Now, fix the back-pointer of the old first node.
                old_first->prev.store(new_node_ptr, std::memory_order_release);
                return;
            }
        }
    }

    // Removes the last element (before the tail sentinel) from the list.
    // Used for LRU eviction. Returns the node for potential reuse.
    Node* pop_back() {
        folly::hazptr_holder h_node, h_prev;
        while (true) {
            Node* last = get_ptr(tail_.prev.load(std::memory_order_acquire));
            h_node.protect(last);

            if (last == &head_) {
                return nullptr; // List is empty
            }

            // Re-validate after protection
            if (get_ptr(tail_.prev.load(std::memory_order_relaxed))!= last) {
                continue;
            }

            if (unlink(last)) {
                return last;
            }
            // Unlink failed, which means another thread is already unlinking 'last'.
            // The loop will retry with the new last node.
        }
    }
};

#endif //LOCKFREELRULIST_HPP
