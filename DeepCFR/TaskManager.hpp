//
// Created by elijah on 8/5/25.
//

#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

#include "TaskSystem.hpp"
#include "GPUDispatcher.hpp" // Assume this is implemented as per the previous answer

template<typename GameType>
class TaskManager {
public:
    TaskManager(
        size_t num_threads,
        GPUDispatcher& dispatcher,
        std::array<AdvantageMemoryBuffer<GameType>, 2>& adv_memories,
        std::vector<TrainingSampleStrategy>& strat_memory,
        std::mt19937& main_rng
    );
    ~TaskManager();

    // Start the worker threads
    void start();

    // Stop the workers and wait for them to finish
    void stop();

    // Add a new task to the ready queue
    void submit_task(std::shared_ptr<Task<GameType>> task);

    // Wait until a certain number of traversals are complete
    void wait_for_completion(int num_traversals);

private:
    void worker_loop();
    void process_task(std::shared_ptr<Task<GameType>> task, std::mt19937& local_rng);

    // Helper function for adding to memory buffers safely
    void add_advantage_sample(int player, TrainingSampleAdvantage&& sample);
    void add_strategy_sample(TrainingSampleStrategy&& sample);

    size_t m_num_threads;
    std::vector<std::thread> m_workers;
    ThreadSafeQueue<std::shared_ptr<Task<GameType>>> m_ready_queue;

    std::atomic<bool> m_stop_flag{false};
    std::atomic<int> m_traversals_completed{0};
    std::condition_variable m_completion_cond;
    std::mutex m_completion_mutex;

    // References to shared resources
    GPUDispatcher& m_dispatcher;
    std::array<AdvantageMemoryBuffer<GameType>, 2>& m_adv_memories;
    std::vector<TrainingSampleStrategy>& m_strategy_memory;
    std::mutex m_memory_mutex; // Mutex to protect shared memory buffers
    std::mt19937& m_rng;
};

#endif // TASKMANAGER_HPP

