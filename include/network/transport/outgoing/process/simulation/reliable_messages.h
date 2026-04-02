#pragma once

#include <deque> // std::deque

#include "network/state.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"

/**
 * @brief Traite et envoie les messages reliable produits par la simulation.
 *
 * Cette fonction parcourt les messages reliable classifiés pour le tick courant,
 * résout le peer cible de chaque connexion puis délègue l'envoi effectif aux
 * routines ENet spécialisées.
 *
 * @param networkState État réseau global du client.
 * @param reliableMessages Messages reliable à envoyer.
 */
void ClientNetworkOutgoing_ProcessSimulationDispatcher_HandleReliableMessages(
    const NetworkState& networkState,
    const std::deque<SimulationToNetworkOUTMessage>& reliableMessages);
