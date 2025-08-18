//
// Created by Elijah Crain on 8/17/25.
//

#ifndef INC_2PLAYERCFR_CFRTASK_HPP
#define INC_2PLAYERCFR_CFRTASK_HPP
#include <coroutine>
#include <cstdlib>

struct CFRTask {
    struct promise_type {
        float returned_value;
        float yielded_value;

        CFRTask get_return_object()
        {
            return CFRTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend()
        {
            return std::suspend_never{};// eagerly run coroutine body
        }
        auto final_suspend() noexcept
        {
            return std::suspend_always{};
        }
        void unhandled_exception()
        {
            std::abort();
        }
        void return_value(float value)
        {
            returned_value = value;
        }
        auto yield_value(float value)
        {
            yielded_value = value;
            return std::suspend_always{};
        }
    };
    std::coroutine_handle<promise_type> handle;
    ~CFRTask()
    {
        if (handle) {
            handle.destroy();
        }
    }
    float get_returned_value() const
    {
        return handle.promise().returned_value;
    }
    float get_yielded_value() const
    {
        return handle.promise().yielded_value;
    }
    bool resume()
    {
        handle.resume();
        return handle.done();
    }

};

#endif //INC_2PLAYERCFR_CFRTASK_HPP