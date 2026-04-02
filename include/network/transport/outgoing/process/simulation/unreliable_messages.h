#pragma once

#include "network/state.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"

/**
 * @brief Traite les messages unreliable issus de la simulation.
 *
 * Cette fonction envoie les derniers messages unreliable retenus pour le tick.
 *
 * @param networkState État réseau global du client.
 * @param hasInputUnreliable Indique si un message input unreliable est disponible.
 * @param lastInputUnreliable Dernier message input unreliable retenu.
 * @param hasClockSyncUnreliable Indique si un message clock sync unreliable est disponible.
 * @param lastClockSyncUnreliable Dernier message clock sync unreliable retenu.
 */
void ClientNetworkOutgoing_ProcessSimulationDispatcher_HandleUnreliableMessages(
    const NetworkState& networkState,
    bool hasInputUnreliable,
    const SimulationToNetworkOUTMessage& lastInputUnreliable,
    bool hasClockSyncUnreliable,
    const SimulationToNetworkOUTMessage& lastClockSyncUnreliable);
