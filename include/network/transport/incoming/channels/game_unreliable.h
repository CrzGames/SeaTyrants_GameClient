#pragma once

#include <cstdint> // uint32_t

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un événement ENet reçu sur le channel gameplay unreliable.
 *
 * Ce handler lit le type de paquet unreliable envoyé par le serveur, désérialise le payload attendu,
 * puis redirige le traitement vers le handler de paquet gameplay unreliable
 * approprié.
 *
 * @param event Événement ENet de réception contenant le paquet brut.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer le message
 *        du thread réseau vers le thread simulation.
 */
void ClientNetworkIncoming_Channel_GameUnreliable(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue);