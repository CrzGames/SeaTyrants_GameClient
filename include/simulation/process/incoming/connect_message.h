#pragma once

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"

/**
 * @brief Traite un message de connexion provenant du thread réseau entrant.
 *
 * Cette fonction prepare et queue le secure-session hello du client.
 *
 * @param networkState Etat reseau global du client.
 * @param simToNetQueue Queue simulation -> reseau pour envoyer le hello.
 * @param networkInToSimMessage Message réseau entrant de type connexion.
 */
void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleConnectMessage(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const NetworkINToSimulationMessage& networkInToSimMessage);
