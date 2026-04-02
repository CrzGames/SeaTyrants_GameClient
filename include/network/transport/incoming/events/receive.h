#pragma once

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un événement ENet de type réception de paquet.
 *
 * Cette fonction valide d'abord le peer source de l'evenement (doit etre
 * le peer serveur actif), puis delegue le traitement au systeme de
 * dispatch par channel.
 *
 * @param event Événement ENet de type receive.
 * @param networkState Etat reseau global utilise pour valider le peer source.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer les messages
 *        du thread réseau vers le thread simulation.
 */
void ClientNetworkIncoming_Event_HandleReceive(
    const ENetEvent* event,
    const NetworkState& networkState,
    NetworkINToSimulationQueue& netToSimQueue);
