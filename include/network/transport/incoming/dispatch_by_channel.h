#pragma once

#include <cstdint> // uint32_t

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Dispatch un événement ENet reçu vers le handler correspondant au channel réseau.
 *
 * Cette fonction analyse le champ `channelID` du paquet ENet reçu afin de
 * sélectionner le bon sous-handler :
 * - secure session reliable
 * - auth reliable
 * - gameplay reliable
 * - gameplay unreliable
 *
 * Elle applique également les vérifications d'autorisation de haut niveau
 * avant de déléguer le traitement au handler du channel ciblé.
 *
 * @param event Événement ENet de réception contenant le paquet brut.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer les messages
 *        du thread réseau vers le thread simulation.
 */
void ClientNetworkIncoming_DispatchByChannel(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue);