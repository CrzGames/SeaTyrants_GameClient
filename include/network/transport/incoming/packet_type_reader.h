#pragma once

#include <rcenet/RCENET_enet.h> // ENetEvent

#include "network/packets/server/reliable.h"
#include "network/packets/server/unreliable.h"

/**
 * @brief Lit le type d'un paquet server reliable depuis le payload ENet.
 *
 * Cette fonction lit le premier octet du buffer réseau associé à l'événement
 * ENet et l'interprète comme une valeur de `ServerReliablePacketType`.
 *
 * @param event Événement ENet de réception contenant le paquet brut.
 * @param outType Paramètre de sortie recevant le type de paquet lu si la lecture réussit.
 *
 * @return `true` si le type de paquet a pu être lu correctement,
 *         `false` si le buffer est vide, invalide ou trop court.
 */
bool ClientNetworkIncoming_ReadServerReliablePacketType(
    const ENetEvent* event,
    ServerReliablePacketType& outType);

/**
 * @brief Lit le type d'un paquet server unreliable depuis le payload ENet.
 *
 * Cette fonction lit le premier octet du buffer réseau associé à l'événement
 * ENet et l'interprète comme une valeur de `ServerUnreliablePacketType`.
 *
 * @param event Événement ENet de réception contenant le paquet brut.
 * @param outType Paramètre de sortie recevant le type de paquet lu si la lecture réussit.
 *
 * @return `true` si le type de paquet a pu être lu correctement,
 *         `false` si le buffer est vide, invalide ou trop court.
 */
bool ClientNetworkIncoming_ReadServerUnreliablePacketType(
    const ENetEvent* event,
    ServerUnreliablePacketType& outType);