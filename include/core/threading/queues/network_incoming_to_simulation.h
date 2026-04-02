#pragma once

#include <mutex>   // std::mutex
#include <deque>   // std::deque
#include <cstdint> // uint16_t, uint32_t, etc.

#include "network/packets/server/unreliable.h"
#include "network/packets/server/reliable.h"

// ======================================================================================
// Queues de messages entre le réseau et la simulation (Network IN -> Simulation)
// ======================================================================================
enum class NetworkINToSimulationMessageType : uint8_t 
{ 
    SERVER_SECURE_SESSION_HELLO_RESPONSE_PACKET_RELIABLE = 0,
    SERVER_AUTH_RESPONSE_PACKET_RELIABLE = 1,
    SERVER_EVENT_CONNECT = 2,
    SERVER_EVENT_DISCONNECT = 3, 
    SERVER_SNAPSHOT_FULL_PACKET_UNRELIABLE = 4
};

struct NetworkINToSimulationMessage
{
    // Type de message (connect, disconnect, input, etc.)
    NetworkINToSimulationMessageType type;

    // type = SERVER_SECURE_SESSION_HELLO_RESPONSE_PACKET_RELIABLE
    ServerSecureSessionHelloResponsePacketReliable secureSessionHelloResponsePacket;

    // type = SERVER_AUTH_RESPONSE_PACKET_RELIABLE
    ServerAuthResponsePacketReliable authResponsePacket;

    // type = SERVER_SNAPSHOT_FULL_PACKET_UNRELIABLE
    ServerSnapshotFullPacketUnreliable snapshotFullPacket;
};

struct NetworkINToSimulationQueue
{
    std::mutex mtx;
    std::deque<NetworkINToSimulationMessage> q;

    void push(const NetworkINToSimulationMessage& m)
    {
        std::lock_guard<std::mutex> lock(mtx);
        q.push_back(m);
    }

    // drain en une fois (moins de lock)
    void drain(std::deque<NetworkINToSimulationMessage>& out)
    {
        std::lock_guard<std::mutex> lock(mtx);
        out.swap(q);
    }
};
