#pragma once

#include "network/state.h"
#include "core/threading/queues/http_to_simulation.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"

/**
 * @brief Traite une reponse HTTP de signin.
 *
 * Cette fonction met a jour l'etat client apres reponse backend.
 *
 * @param networkState Etat reseau global du client.
 * @param simToNetQueue Queue simulation -> reseau (disponible si un envoi reseau est necessaire).
 * @param httpToSimMessage Message HTTP entrant contenant la reponse signin.
 */
void ClientSimulation_ProcessHttpDispatcher_HandleAuthSignInResponseMessage(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const HttpToSimulationMessage& httpToSimMessage);
