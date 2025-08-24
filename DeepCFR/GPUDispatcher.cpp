//
// Created by elijah on 8/5/25.
//

#include "GPUDispatcher.hpp"

void GPUDispatcher::run_loop() {
    std::vector<std::unique_ptr<ForwardRequest>> p0_requests;
    std::vector<std::unique_ptr<ForwardRequest>> p1_requests;

    while (!m_stop_flag) {
        // Wait for a request with a timeout
        auto request_opt = m_input_queue.pop_with_timeout(MAX_WAIT_TIME);

        if (request_opt.has_value()) {
            auto& request = *request_opt;
            if (request->player_index == 0) {
                p0_requests.push_back(std::move(request));
            } else {
                p1_requests.push_back(std::move(request));
            }
        }

        // Process a batch if it's full or if we timed out (request_opt is null) and the batch isn't empty
        if (p0_requests.size() >= MAX_BATCH_SIZE || (!request_opt.has_value() && !p0_requests.empty())) {
            process_batch(p0_requests, m_advantage_networks[0]);
            p0_requests.clear();
        }
        if (p1_requests.size() >= MAX_BATCH_SIZE || (!request_opt.has_value() && !p1_requests.empty())) {
            process_batch(p1_requests, m_advantage_networks[1]);
            p1_requests.clear();
        }
    }
    // Process any remaining requests after stop is called
    if (!p0_requests.empty()) process_batch(p0_requests, m_advantage_networks[0]);
    if (!p1_requests.empty()) process_batch(p1_requests, m_advantage_networks[1]);
}

void GPUDispatcher::process_batch(std::vector<std::unique_ptr<ForwardRequest>>& batch, DeepCFRModel& network) {
    if (batch.empty()) return;

    size_t batch_size = batch.size();
    int num_card_types = batch[0]->cards.size();

    // Batch the tensors
    std::vector<torch::Tensor> batched_cards;
    std::vector<torch::Tensor> bet_list;
    batched_cards.reserve(num_card_types);
    bet_list.reserve(batch_size);

    for (int i = 0; i < num_card_types; ++i) {
        std::vector<torch::Tensor> card_type_list;
        card_type_list.reserve(batch_size);
        for (const auto& req : batch) {
            card_type_list.push_back(req->cards[i]);
        }
        // FIX: Squeeze the dimension of size 1 that torch::stack adds.
        // This changes the shape from [batch_size, 1, features] to [batch_size, features].
        batched_cards.push_back(torch::stack(card_type_list).squeeze(1).to(m_device));
    }

    for (const auto& req : batch) {
        bet_list.push_back(req->bets);
    }
    // FIX: Squeeze the dimension of size 1 for the bets tensor as well.
    auto batched_bets = torch::stack(bet_list).squeeze(1).to(m_device);

    // Run inference on the batch
    torch::Tensor results;
    {
        torch::NoGradGuard no_grad;
        results = network->forward(batched_cards, batched_bets);
    }

    for (size_t i = 0; i < batch.size(); ++i) {
        auto handle = batch[i]->handle_to_resume;
        if (handle) {
            // Place the result directly into the coroutine's promise object [cite: 30]
            handle.promise().m_gpu_result = results.slice(0, i, i + 1).to(torch::kCPU);

            // Push the handle to the results queue for the main thread to resume
            m_results_queue.push(handle);
        }
    }
}
