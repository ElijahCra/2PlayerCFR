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

    // Sentinel node structure without value (for head/tail)
    struct SentinelNode {
        std::atomic<uintptr_t> next;
        std::atomic<uintptr_t> prev;
        
        SentinelNode() : next(0), prev(0) {}
    };
    
    // Aligned sentinel nodes to simplify boundary conditions.
    alignas(folly::hardware_destructive_interference_size) SentinelNode head_;
    alignas(folly::hardware_destructive_interference_size) SentinelNode tail_;
    
    // Size tracking
    std::atomic<size_t> size_{0};

    // Helper functions for marked pointer manipulation
    static inline Node* get_ptr(uintptr_t p) {
        return reinterpret_cast<Node*>(p & ~MARK_BIT);
    }
    
    static inline SentinelNode* get_sentinel_ptr(uintptr_t p) {
        return reinterpret_cast<SentinelNode*>(p & ~MARK_BIT);
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
        // Simplified implementation without hazptr for now
        // In production, proper hazptr protection would be needed
        
        // Find the first non-marked predecessor.
        Node* prev = get_ptr(node->prev.load());
        while (prev && is_marked(prev->next.load())) {
            prev = get_ptr(prev->prev.load());
        }
        
        if (!prev) return;

        // Swing predecessor's next pointer to bypass the node.
        Node* next_node = get_ptr(node->next.load());
        uintptr_t expected_node_ptr = reinterpret_cast<uintptr_t>(node);
        uintptr_t next_node_ptr = reinterpret_cast<uintptr_t>(next_node);

        // This CAS is the primary step of physical unlinking from the forward chain.
        prev->next.compare_exchange_strong(expected_node_ptr, next_node_ptr, std::memory_order_release);

        // Swing successor's prev pointer to bypass the node.
        if (next_node && get_ptr(next_node->prev.load()) == node) {
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

        // Only provide constructor that takes a value - no default constructor
        explicit Node(T val) : value(std::move(val)), next(0), prev(0) {}
        
        template<typename... Args>
        explicit Node(Args&&... args) : value(std::forward<Args>(args)...), next(0), prev(0) {}
    };

    LockFreeLRUList() : head_(), tail_() {
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
        while (true) {
            uintptr_t head_next_val = head_.next.load(std::memory_order_acquire);
            Node* old_first = get_ptr(head_next_val);

            node->next.store(head_next_val, std::memory_order_relaxed);
            node->prev.store(reinterpret_cast<uintptr_t>(&head_), std::memory_order_relaxed);

            uintptr_t new_node_ptr = reinterpret_cast<uintptr_t>(node);

            // Atomically swing the head's next pointer to our new node.
            if (head_.next.compare_exchange_weak(head_next_val, new_node_ptr,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
                // Successfully inserted. Now, fix the back-pointer of the old first node.
                if (old_first) {
                    old_first->prev.store(new_node_ptr, std::memory_order_release);
                }
                return;
            }
        }
    }

    // Removes the last element (before the tail sentinel) from the list.
    // Used for LRU eviction. Returns the node for potential reuse.
    Node* pop_back() {
        while (true) {
            Node* last = get_ptr(tail_.prev.load(std::memory_order_acquire));

            if (reinterpret_cast<SentinelNode*>(last) == &head_) {
                return nullptr; // List is empty
            }

            if (unlink(last)) {
                size_.fetch_sub(1, std::memory_order_relaxed);
                return last;
            }
            // Unlink failed, which means another thread is already unlinking 'last'.
            // The loop will retry with the new last node.
        }
    }

    // Iterator class for range-based for loops
    class iterator {
    private:
        Node* node_;
        
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        
        // Default constructor for STL compatibility
        iterator() : node_(nullptr) {}
        
        explicit iterator(Node* node) : node_(node) {}
        
        reference operator*() const { 
            if (!node_) throw std::runtime_error("Dereferencing null iterator");
            return node_->value; 
        }
        pointer operator->() const { 
            if (!node_) throw std::runtime_error("Dereferencing null iterator");
            return &node_->value; 
        }
        
        iterator& operator++() {
            if (node_) {
                node_ = get_ptr(node_->next.load(std::memory_order_acquire));
                // Skip marked nodes
                while (node_ && is_marked(reinterpret_cast<uintptr_t>(node_))) {
                    node_ = get_ptr(node_->next.load(std::memory_order_acquire));
                }
            }
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(const iterator& other) const { return node_ == other.node_; }
        bool operator!=(const iterator& other) const { return node_ != other.node_; }
        
        Node* get_node() const { return node_; }
        
        bool is_valid() const { return node_ != nullptr; }
    };
    
    // STL-compatible interface methods
    size_t size() const {
        return size_.load(std::memory_order_relaxed);
    }
    
    bool empty() const {
        return size() == 0;
    }
    
    iterator begin() {
        Node* first = get_ptr(head_.next.load(std::memory_order_acquire));
        // Skip to the first non-sentinel, non-marked node
        while (reinterpret_cast<SentinelNode*>(first) != &tail_ && is_marked(reinterpret_cast<uintptr_t>(first))) {
            first = get_ptr(first->next.load(std::memory_order_acquire));
        }
        return iterator(reinterpret_cast<SentinelNode*>(first) == &tail_ ? nullptr : first);
    }
    
    iterator end() {
        return iterator(nullptr);
    }
    
    // Emplace front method
    iterator emplace_front(T&& value) {
        Node* node = new Node(std::forward<T>(value));
        push_front(node);
        size_.fetch_add(1, std::memory_order_relaxed);
        return iterator(node);
    }
    
    template<typename... Args>
    iterator emplace_front(Args&&... args) {
        Node* node = new Node(std::forward<Args>(args)...);
        push_front(node);
        size_.fetch_add(1, std::memory_order_relaxed);
        return iterator(node);
    }
    
    // Erase method that takes an iterator
    iterator erase(iterator it) {
        if (it == end()) {
            return end();
        }
        
        Node* node = it.get_node();
        // Get next node before unlinking
        Node* next_node = get_ptr(node->next.load(std::memory_order_acquire));
        while (reinterpret_cast<SentinelNode*>(next_node) != &tail_ && is_marked(reinterpret_cast<uintptr_t>(next_node))) {
            next_node = get_ptr(next_node->next.load(std::memory_order_acquire));
        }
        
        if (unlink(node)) {
            size_.fetch_sub(1, std::memory_order_relaxed);
            // Schedule for deletion via hazptr
            node->retire();
        }
        
        return iterator(reinterpret_cast<SentinelNode*>(next_node) == &tail_ ? nullptr : next_node);
    }
    
    // Get reference to back element
    T& back() {
        Node* last = get_ptr(tail_.prev.load(std::memory_order_acquire));
        if (reinterpret_cast<SentinelNode*>(last) == &head_) {
            throw std::runtime_error("back() called on empty list");
        }
        return last->value;
    }
    
    const T& back() const {
        Node* last = get_ptr(tail_.prev.load(std::memory_order_acquire));
        if (reinterpret_cast<SentinelNode*>(last) == &head_) {
            throw std::runtime_error("back() called on empty list");
        }
        return last->value;
    }
    
    // Clear all elements
    void clear() {
        while (!empty()) {
            Node* node = pop_back();
            if (node) {
                node->retire();
            }
        }
    }
};

#endif //LOCKFREELRULIST_HPP
