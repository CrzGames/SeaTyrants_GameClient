#pragma once

#include <mutex>   // std::mutex
#include <deque>   // std::deque
#include <cstdint> // uint16_t, uint32_t, etc.

#include "services/http/types/auth/responses.h"

// ======================================================================================
// Messages du thread HTTP vers la simulation (HTTP -> Simulation)
// ======================================================================================

enum class HttpToSimulationMessageType : uint8_t
{
    AUTH_SIGNUP_RESPONSE = 0,
    AUTH_SIGNIN_RESPONSE = 1,
};

struct HttpToSimulationMessage
{
    HttpToSimulationMessageType type;

    // Pour le type AUTH_SIGNUP_RESPONSE
    AuthSignUpHTTPResponse authSignUpResponse;

    // Pour le type AUTH_SIGNIN_RESPONSE
    AuthSignInHTTPResponse authSignInResponse;
};

struct HttpToSimulationQueue
{
    std::mutex mtx;
    std::deque<HttpToSimulationMessage> q;

    void push(const HttpToSimulationMessage& m)
    {
        std::lock_guard<std::mutex> lock(mtx);
        q.push_back(m);
    }

    void drain(std::deque<HttpToSimulationMessage>& out)
    {
        std::lock_guard<std::mutex> lock(mtx);
        out.swap(q);
    }
};
