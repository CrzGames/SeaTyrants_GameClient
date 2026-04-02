#pragma once

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite la reponse d'authentification envoyee par le serveur.
 *
 * Met a jour deux flags distincts:
 * - `authTokenValidated`  : le serveur a accepte le token (status SUCCESS).
 */
void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleAuthResponseMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage);
