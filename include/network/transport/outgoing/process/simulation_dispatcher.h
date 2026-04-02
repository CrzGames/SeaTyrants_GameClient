#pragma once

#include "network/state.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"
#include "network/transport/outgoing/message_preparation.h"

/**
 * @brief Dispatch le traitement des messages simulation -> réseau sortant.
 *
 * Cette fonction reçoit les messages déjà classifiés entre reliable et
 * unreliable, puis délègue leur traitement aux handlers spécialisés
 * correspondants.
 *
 * @param networkState État réseau global du client.
 * @param reliableMessages Messages reliable à traiter.
 * @param preparedMessages Messages prepared (reliable + unreliable coalesces)
 *        pour le peer serveur unique.
 */
void ClientNetworkOutgoing_ProcessSimulationDispatcher(
    NetworkState& networkState,
    const ClientNetworkOutgoingPreparedMessages& preparedMessages);
