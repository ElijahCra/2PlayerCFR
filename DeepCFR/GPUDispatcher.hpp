//
// Created by elijah on 8/5/25.
//

#ifndef GPUDISPATCHER_HPP
#define GPUDISPATCHER_HPP

#include <thread>
#include <atomic>
#include <future>

#include "types.hpp" // For ForwardRequest and ThreadSafeQueue
#include "Net.hpp"      // For DeepCFRModel

class GPUDispatcher {
public:
    GPUDispatcher(std::array<DeepCFRModel, 2>& advantage_networks,
                      ThreadSafeQueue<std::coroutine_handle<>>& results_queue,
                      torch::Device device)
            : m_advantage_networks(advantage_networks),
              m_results_queue(results_queue), // Store reference to the results queue
              m_device(device),
              m_stop_flag(false) {}

    ~GPUDispatcher() {
        stop();
    }

    void submit(std::unique_ptr<ForwardRequest> request) {
        m_input_queue.push(std::move(request));
    }

    void start() {
        m_gpu_thread = std::thread(&GPUDispatcher::run_loop, this);
    }

    void stop() {
        if (m_stop_flag.exchange(true)) return; // Already stopped
        if (m_gpu_thread.joinable()) {
            m_gpu_thread.join();
        }
    }

private:
    void run_loop();
    void process_batch(std::vector<std::unique_ptr<ForwardRequest>>& batch, DeepCFRModel& network);
    // Configuration
    const size_t MAX_BATCH_SIZE = 64;
    const std::chrono::milliseconds MAX_WAIT_TIME{5};

    // Member variables
    std::array<DeepCFRModel, 2>& m_advantage_networks;
    torch::Device m_device;
    ThreadSafeQueue<std::unique_ptr<ForwardRequest>> m_input_queue;
    ThreadSafeQueue<std::coroutine_handle<>>& m_results_queue; // This is new
    std::thread m_gpu_thread;
    std::atomic<bool> m_stop_flag;
};




#endif //GPUDISPATCHER_HPP
