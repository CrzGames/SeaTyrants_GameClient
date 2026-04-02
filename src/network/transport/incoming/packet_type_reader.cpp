#include "network/transport/incoming/packet_type_reader.h"

#include "network/serialization/byte_reader.h"

bool ClientNetworkIncoming_ReadServerReliablePacketType(
    const ENetEvent* event,
    ServerReliablePacketType& outType)
{
    // Crée un ByteReader pour lire les données du paquet reçu.
    ByteReader reader(event->packet->data, event->packet->dataLength);

    // Variable temporaire brute qui recevra le premier octet du packet.
    uint8_t rawType = 0;

    // Tente de lire un octet du paquet pour obtenir le type brut du packet.
    if (!reader.readU8(rawType))
    {
        // Retourne false si la lecture échoue, indiquant que le type de paquet n'a pas pu être déterminé.
        return false;
    }

    // Convertit le type brut en un type de paquet fiable spécifique à l'application.
    outType = static_cast<ServerReliablePacketType>(rawType);

    // Retourne true pour indiquer que le type de paquet a été lu avec succès.
    return true;
}

bool ClientNetworkIncoming_ReadServerUnreliablePacketType(
    const ENetEvent* event,
    ServerUnreliablePacketType& outType)
{
    // Crée un ByteReader pour lire les données du paquet reçu.
    ByteReader reader(event->packet->data, event->packet->dataLength);

    // Variable temporaire brute qui recevra le premier octet du packet.
    uint8_t rawType = 0;

    // Tente de lire un octet du paquet pour obtenir le type brut du packet.
    if (!reader.readU8(rawType))
    {
        // Retourne false si la lecture échoue, indiquant que le type de paquet n'a pas pu être déterminé.
        return false;
    }

    // Convertit le type brut en un type de paquet non fiable spécifique à l'application.
    outType = static_cast<ServerUnreliablePacketType>(rawType);

    // Retourne true pour indiquer que le type de paquet a été lu avec succès.
    return true;
}