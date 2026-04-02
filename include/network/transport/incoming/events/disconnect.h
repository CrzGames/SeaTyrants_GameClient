#pragma once

#include <rcenet/RCENET_enet.h> // EnetEvent

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un événement ENet de type déconnexion.
 *
 * Cette fonction récupère l'identifiant de connexion associé au peer,
 * supprime le mapping dans l'état réseau global, nettoie `peer->data`,
 * puis pousse un message de déconnexion dans la queue réseau -> simulation.
 *
 * @param event Événement ENet de type disconnect.
 * @param networkState État réseau global du client, modifié pour retirer
 *        la connexion fermée.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer le message
 *        de déconnexion vers le thread simulation.
 */
void ClientNetworkIncoming_Event_HandleDisconnect(
    const ENetEvent* event,
    NetworkState& networkState,
    NetworkINToSimulationQueue& netToSimQueue);