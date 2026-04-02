#pragma once

#include <cstdint> // uint16_t, uint32_t, etc.
#include <deque>   // std::deque
#include <mutex>   // std::mutex, std::lock_guard
#include <string>  // std::string

// ======================================================================================
// Messages du thread WebSocket vers la simulation (WebSocket -> Simulation)
// ======================================================================================

enum class WebSocketToSimulationMessageType : uint8_t
{
    // Ajouter des types de messages ici si besoin
};

struct WebSocketToSimulationMessage
{
    // Type de message.
    WebSocketToSimulationMessageType type;

    // Ajouter des champs de données spécifiques au message ici si besoin
};

struct WebSocketToSimulationQueue
{
    std::mutex mtx;
    std::deque<WebSocketToSimulationMessage> q;

    void push(const WebSocketToSimulationMessage& m)
    {
        std::lock_guard<std::mutex> lock(mtx);
        q.push_back(m);
    }

    void drain(std::deque<WebSocketToSimulationMessage>& out)
    {
        std::lock_guard<std::mutex> lock(mtx);
        out.swap(q);
    }
};
