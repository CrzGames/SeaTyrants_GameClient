#pragma once

#include <deque> // std::deque

#include "core/threading/queues/simulation_to_network_outgoing.h"

/**
 * @brief Draine la queue simulation -> réseau sortant vers une deque locale.
 *
 * Cette fonction vide la queue thread-safe alimentée par la simulation
 * et copie les messages extraits dans un conteneur local afin qu'ils puissent
 * être classifiés puis envoyés pendant le tick réseau sortant courant.
 *
 * @param simToNetQueue Queue thread-safe simulation -> réseau sortant.
 * @param outMessages Deque de sortie recevant les messages drainés.
 */
void ClientNetworkOutgoing_DrainSimulationToNetworkOutgoingQueue(
    SimulationToNetworkOUTQueue& simToNetQueue,
    std::deque<SimulationToNetworkOUTMessage>& outMessages);