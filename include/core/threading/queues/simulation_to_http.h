#pragma once

#include <mutex>   // std::mutex
#include <deque>   // std::deque
#include <cstdint> // uint16_t, uint32_t, etc.
#include <condition_variable> // std::condition_variable, std::unique_lock

#include "services/http/types/auth/requests.h"

// ======================================================================================
// Messages de la simulation vers le thread HTTP (Simulation -> HTTP)
// ======================================================================================

enum class SimulationToHttpMessageType : uint8_t
{
    AUTH_SIGNUP_REQUEST = 0,
    AUTH_SIGNIN_REQUEST = 1,
};

struct SimulationToHttpMessage
{
    SimulationToHttpMessageType type;

    // Pour le type AUTH_SIGNUP_REQUEST
    AuthSignUpHTTPRequest authSignUpRequest;

    // Pour le type AUTH_SIGNIN_REQUEST
    AuthSignInHTTPRequest authSignInRequest;
};

struct SimulationToHttpQueue
{
    std::mutex mtx;
    std::condition_variable cv;
    std::deque<SimulationToHttpMessage> q;
    bool stopped = false;

    void push(const SimulationToHttpMessage& m)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push_back(m);
        }
        cv.notify_one();
    }

    bool waitAndPop(SimulationToHttpMessage& out)
    {
        std::unique_lock<std::mutex> lock(mtx);

        cv.wait(lock, [this] {
            return stopped || !q.empty();
        });

        if (stopped && q.empty())
        {
            return false;
        }

        out = std::move(q.front());
        q.pop_front();
        return true;
    }

    void stop(void)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stopped = true;
        }
        cv.notify_all();
    }

    void reset(void)
    {
        std::lock_guard<std::mutex> lock(mtx);
        stopped = false;
        q.clear();
    }
};
