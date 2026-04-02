#pragma once

#include <cstdint> // uint32_t

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un événement ENet reçu sur le channel d'établissement de session sécurisée.
 *
 * Ce handler est responsable du traitement initial des paquets reliable liés
 * à l'ouverture de session sécurisée côté serveur. Il désérialise le paquet
 * reçu, valide les informations critiques du protocole, puis envoie un
 * message vers la simulation si le contenu est valide.
 *
 * @param event Événement ENet de réception contenant le paquet brut.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer le message
 *        du thread réseau vers le thread simulation.
 */
void ClientNetworkIncoming_Channel_SecureSessionReliable(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue);