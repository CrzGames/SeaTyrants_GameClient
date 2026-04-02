#pragma once

#include <condition_variable> // std::condition_variable, std::unique_lock
#include <cstdint>            // uint16_t, uint32_t, etc.
#include <deque>              // std::deque
#include <mutex>              // std::mutex, std::lock_guard
#include <string>             // std::string

// ======================================================================================
// Messages de la simulation vers le thread WebSocket (Simulation -> WebSocket)
// ======================================================================================

enum class SimulationToWebSocketMessageType : uint8_t
{
    // Ajouter des types de messages ici si besoin
};

struct SimulationToWebSocketMessage
{
    SimulationToWebSocketMessageType type;

    // Ajouter des champs de données spécifiques au message ici si besoin
};

struct SimulationToWebSocketQueue
{
    std::mutex mtx;
    std::condition_variable cv;
    std::deque<SimulationToWebSocketMessage> q;
    bool stopped = false;

    void push(const SimulationToWebSocketMessage& m)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push_back(m);
        }
        cv.notify_one();
    }

    bool waitAndPop(SimulationToWebSocketMessage& out)
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
