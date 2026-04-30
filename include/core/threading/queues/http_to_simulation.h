#pragma once

#include <cstdint> // uint8_t
#include <deque>   // std::deque
#include <mutex>   // std::mutex

#include "services/http/types/auth/responses.h"

// ======================================================================================
// Messages du thread HTTP vers la simulation (HTTP -> Simulation)
// ======================================================================================

enum class HttpToSimulationMessageType : uint8_t
{
    AUTH_SIGNIN_RESPONSE = 0,
};

struct HttpToSimulationMessage
{
    HttpToSimulationMessageType type;

    // Payload utilise lorsque `type == AUTH_SIGNIN_RESPONSE`.
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
