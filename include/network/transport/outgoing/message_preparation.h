#pragma once

#include <deque> // std::deque

#include "core/threading/queues/simulation_to_network_outgoing.h"

/**
 * @brief Regroupe les messages sortants préparés pour le tick réseau sortant courant.
 *
 * Cette structure contient les messages issus de la simulation après préparation
 * pour le tick réseau sortant courant.
 *
 * Politique de conservation :
 * - les messages reliable sont tous conservés dans leur ordre de production ;
 * - certains messages unreliable sont coalesces par famille, en ne gardant
 *   que le dernier message pertinent pour le peer serveur.
 *
 * Contenu actuel :
 * - `reliableMessages` :
 *   tous les messages reliable à envoyer dans l'ordre ;
 */
struct ClientNetworkOutgoingPreparedMessages
{
    std::deque<SimulationToNetworkOUTMessage> reliableMessages;
    bool hasInputUnreliable = false;
    SimulationToNetworkOUTMessage lastInputUnreliable{};
    bool hasClockSyncUnreliable = false;
    SimulationToNetworkOUTMessage lastClockSyncUnreliable{};
};

void ClientNetworkOutgoing_SplitReliableAndCoalesceUnreliableMessages(
    const std::deque<SimulationToNetworkOUTMessage>& simToNetOutMessages,
    ClientNetworkOutgoingPreparedMessages& preparedMessages);
