#pragma once

#include <rcenet/RCENET_enet.h> // For ENetEvent

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un événement ENet de type connexion.
 *
 * Cette fonction crée un nouvel identifiant de connexion, attache cet ID
 * au peer ENet, enregistre le mapping dans l'état réseau global et pousse
 * un message de connexion dans la queue réseau -> simulation.
 *
 * @param event Événement ENet de type connect.
 * @param networkState État réseau global du client, modifié pour enregistrer
 *        la nouvelle connexion.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer le message
 *        de connexion vers le thread simulation.
 */
void ClientNetworkIncoming_Event_HandleConnect(
    const ENetEvent* event,
    NetworkState& networkState,
    NetworkINToSimulationQueue& netToSimQueue);