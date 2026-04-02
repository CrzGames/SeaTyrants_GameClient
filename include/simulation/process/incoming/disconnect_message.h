#pragma once

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un message de déconnexion provenant du thread réseau entrant.
 *
 * Cette fonction nettoie l'etat secure-session/auth/chiffrement du client.
 *
 * @param networkState Etat reseau global du client.
 * @param networkInToSimMessage Message réseau entrant de type déconnexion.
 */
void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleDisconnectMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage);
