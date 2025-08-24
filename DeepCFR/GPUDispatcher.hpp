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
    GPUDispatcher(std::array<DeepCFRModel, 2>& advantage_networks, torch::Device device)
        : m_advantage_networks(advantage_networks), m_device(device), m_stop_flag(false) {}

    ~GPUDispatcher() {
        stop();
    }

    // Submit a request and get a future for the result
    std::future<torch::Tensor> submit(std::unique_ptr<ForwardRequest> request) {
        auto future = request->promise.get_future();
        m_queue.push(std::move(request));
        return future;
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
    ThreadSafeQueue<std::unique_ptr<ForwardRequest>> m_queue;
    std::thread m_gpu_thread;
    std::atomic<bool> m_stop_flag;
};




#endif //GPUDISPATCHER_HPP
