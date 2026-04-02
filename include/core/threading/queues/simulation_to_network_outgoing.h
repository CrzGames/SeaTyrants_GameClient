#pragma once

#include <mutex>   // std::mutex
#include <deque>   // std::deque
#include <cstdint> // uint16_t, uint32_t, etc.
#include <vector>  // std::vector

// ======================================================================================
// Messages de la simulation vers le réseau (Simulation -> Network OUT)
// ======================================================================================
enum class SimulationToNetworkOUTMessageType : uint8_t
{
    CLIENT_SECURE_SESSION_HELLO_PACKET_RELIABLE = 0,
    CLIENT_AUTH_PACKET_RELIABLE = 1,
    CLIENT_READY_FOR_MATCH_PACKET_RELIABLE = 2,
    CLIENT_INPUT_PACKET_UNRELIABLE = 3,
    CLIENT_CLOCK_SYNC_PACKET_UNRELIABLE = 4,
};

struct SimulationToNetworkOUTMessage
{
    // Type de message sortant client.
    SimulationToNetworkOUTMessageType type;

    // payload brut à envoyer (contenant le packet sérialisé correspondant au type de message)
    std::vector<uint8_t> serializedPacket;
};

struct SimulationToNetworkOUTQueue
{
    std::mutex mtx;
    std::deque<SimulationToNetworkOUTMessage> q;

    void push(const SimulationToNetworkOUTMessage& m)
    {
        std::lock_guard<std::mutex> lock(mtx);
        q.push_back(m);
    }

    void drain(std::deque<SimulationToNetworkOUTMessage>& out)
    {
        std::lock_guard<std::mutex> lock(mtx);
        out.swap(q);
    }
};
