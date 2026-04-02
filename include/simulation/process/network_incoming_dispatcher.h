#pragma once

#include <deque> // std::deque

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"
#include "core/threading/queues/simulation_to_network_outgoing.h"
#include "core/threading/queues/simulation_to_http.h"

/**
 * @brief Traite les messages entrants provenant du thread réseau.
 *
 * Cette fonction parcourt les messages déjà drainés depuis la queue
 * réseau -> simulation puis dispatch le traitement en fonction du type
 * de message reçu.
 *
 * @param networkState Etat reseau global du client.
 * @param simToNetQueue Queue simulation -> réseau utilisée pour préparer
 *        les messages reseau sortants du client vers le serveur.
 * @param simToHttpQueue Queue simulation -> HTTP utilisée pour déléguer
 *        les operations HTTP client (signup/signin).
 * @param messages Messages entrants réseau déjà drainés pour le tick courant.
 */
void ClientSimulation_ProcessNetworkIncomingDispatcher(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    SimulationToHttpQueue& simToHttpQueue,
    const std::deque<NetworkINToSimulationMessage>& networkInToSimMessages);
