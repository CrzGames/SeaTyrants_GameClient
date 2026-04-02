#pragma once

#include <cstdint> // uint32_t

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un événement ENet reçu sur le channel d'authentification reliable.
 *
 * Ce handler est responsable du traitement des paquets serveur -> client
 * reçus sur le channel AUTH_RELIABLE. Il désérialise le payload attendu,
 * construit le message réseau -> simulation correspondant, puis le pousse
 * dans la queue de simulation.
 *
 * @param event Événement ENet de réception contenant le paquet brut.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer le message
 *        du thread réseau vers le thread simulation.
 */
void ClientNetworkIncoming_Channel_AuthReliable(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue);