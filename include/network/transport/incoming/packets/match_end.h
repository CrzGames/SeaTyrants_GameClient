#pragma once

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "core/threading/queues/network_incoming_to_simulation.h"

// Handler du packet serveur reliable SERVER_MATCH_END_PACKET_RELIABLE.
// Placeholder: la deserialisation metier sera ajoutee ensuite.
void ClientNetworkIncoming_HandlePacket_MatchEnd(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue);

