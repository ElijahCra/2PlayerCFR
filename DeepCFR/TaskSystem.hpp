//
// Created by elijah on 8/5/25.
//

#ifndef TASKSYSTEM_HPP
#define TASKSYSTEM_HPP

#include <vector>
#include <future>
#include <optional>
#include <functional>
#include <atomic>
#include <mutex>
#include <memory>
#include "torch/torch.h"
#include "../Game/Preflop/Game.hpp" // Or your specific game header
#include "../Game/Texas/Game.hpp"

#include "types.hpp"
#include "AdvantageMemoryBuffer.hpp"

// Forward declarations
template<typename GameType> class TaskManager;
template<typename GameType> struct Task;

// Holds the state for a parent task waiting on children to complete.
// This replaces the role of the call stack in recursion.
template<typename GameType>
struct ParentState {
    std::mutex mtx;
    std::shared_ptr<Task<GameType>> parent_task; // The task to resume when children are done

    // Information needed to resume the parent task
    std::vector<float> strategy;
    std::vector<typename GameType::Action> legal_actions;

    // State for collecting results from children
    std::vector<float> counterfactual_values;
    std::atomic<int> children_to_complete;
    float node_value = 0.0f;

    ParentState(std::shared_ptr<Task<GameType>> parent, const std::vector<float>& strat, const std::vector<typename GameType::Action>& actions)
        : parent_task(parent), strategy(strat), legal_actions(actions) {
        counterfactual_values.resize(actions.size(), 0.0f);
        children_to_complete.store(actions.size());
    }
};

// Represents one unit of work in the traversal.
template<typename GameType>
struct Task {
    // Defines what logic to execute for this task
    enum class Type {
        TRAVERSE_NODE,      // Initial processing of a game node
        RESUME_AFTER_GPU,   // Continue processing after getting a network result
        PROCESS_PARENT,     // Process a parent task after all its children have finished
    } type = Type::TRAVERSE_NODE;

    // Game state
    GameType game_state;
    int update_player;
    int iter;

    // Optional data depending on the task type
    std::optional<torch::Tensor> gpu_result; // For RESUME_AFTER_GPU

    // Parent-child linkage to report results back up the tree
    std::shared_ptr<ParentState<GameType>> parent_state = nullptr;
    int action_index_in_parent = -1; // Which child am I?

};

#endif // TASKSYSTEM_HPP

