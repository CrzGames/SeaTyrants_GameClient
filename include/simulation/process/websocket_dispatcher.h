#pragma once

#include <deque> // std::deque

#include "network/state.h"
#include "core/threading/queues/websocket_to_simulation.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"

/**
 * @brief Traite les messages entrants provenant du thread WebSocket.
 *
 * Cette fonction parcourt les messages déjà drainés depuis la queue
 * WebSocket -> simulation puis dispatch le traitement en fonction du type
 * de message reçu.
 *
 * @param networkState Etat reseau global du client.
 * @param simToNetQueue Queue simulation -> réseau utilisée pour préparer
 *        les reponses reseau envoyees au serveur.
 * @param websocketToSimMessages Messages entrants WebSocket déjà drainés pour le tick courant.
 */
void ClientSimulation_ProcessWebSocketDispatcher(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const std::deque<WebSocketToSimulationMessage>& websocketToSimMessages);
