#pragma once

#include <cstdint> // uint32_t

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "core/threading/queues/network_incoming_to_simulation.h"

/**
 * @brief Traite un paquet gameplay unreliable de type snapshot full serveur.
 *
 * Ce handler désérialise le paquet `ServerSnapshotFullPacketUnreliable`,
 * vérifie qu'il est valide, puis construit le message réseau -> simulation
 * correspondant avant de l'envoyer à la queue de simulation.
 *
 * @param event Événement ENet de réception contenant le paquet brut.
 * @param netToSimQueue Queue thread-safe utilisée pour transférer le message
 *        du thread réseau vers le thread simulation.
 */
void ClientNetworkIncoming_HandlePacket_SnapshotFull(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue);
