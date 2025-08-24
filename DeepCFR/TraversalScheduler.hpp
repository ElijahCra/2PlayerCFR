#pragma once
#include <deque>
#include <coroutine>
#include <functional>

#include "GPUDispatcher.hpp"
#include "CoroutineTask.hpp"

template<typename GameType>
class TraversalScheduler {
public:
    TraversalScheduler(std::function<CoroutineTask<float>()> traversal_factory, size_t max_concurrent_traversals)
      : m_traversal_factory(traversal_factory), 
        m_max_concurrent_traversals(max_concurrent_traversals),
        m_active_traversals(0) {}

    void run(GPUDispatcher& dispatcher) {
        while (m_active_traversals > 0 || !all_initial_traversals_started()) {
            
            // 1. Prioritize resuming traversals with ready GPU results.
            auto handle_opt = dispatcher.get_results_queue().pop_with_timeout(std::chrono::milliseconds(0));
            if (handle_opt.has_value()) {
                (*handle_opt).resume();
                if ((*handle_opt).done()) {
                    m_active_traversals--;
                }
                continue; // Always check for more completed work first
            }

            // 2. If no results are ready, start a new traversal if we have capacity.
            if (m_active_traversals < m_max_concurrent_traversals && !all_initial_traversals_started()) {
                start_new_traversal();
            }

            // 3. If there's pending work that hasn't been suspended yet, run it.
            if (!m_ready_queue.empty()) {
                auto handle = m_ready_queue.front();
                m_ready_queue.pop_front();
                handle.resume(); // This will run until the first co_await
                 if (handle.done()) {
                    m_active_traversals--;
                }
            }
        }
    }

    void start_new_traversal() {
        std::cout << "Starting new traversal." << std::endl;
        CoroutineTask<float> new_task = m_traversal_factory();
        m_ready_queue.push_back(new_task.m_handle);
        new_task.m_handle = nullptr; // The queue now owns the handle
        m_active_traversals++;
    }

private:
    bool all_initial_traversals_started() const {
        // Define your own logic for when to stop creating new traversals
        return m_active_traversals >= m_max_concurrent_traversals;
    }

    std::deque<std::coroutine_handle<>> m_ready_queue;
    std::function<CoroutineTask<float>()> m_traversal_factory;
    size_t m_max_concurrent_traversals;
    size_t m_active_traversals;
};