#include "network/transport/incoming/channels/auth_reliable.h"

#include "network/serialization/deserialize_packets_server.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_Channel_AuthReliable(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Tente de deserialiser le packet envoye par le serveur.
    ServerAuthResponsePacketReliable authResponsePacket{};
    if (!deserializeServerAuthResponsePacketReliable(
            event->packet->data,
            event->packet->dataLength,
            authResponsePacket))
    {
        // Si la deserialisation echoue, le packet est invalide ou mal forme.
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_IN] [AUTH] - Failed to deserialize auth response packet from server (size: %u bytes).",
            static_cast<unsigned>(event->packet->dataLength));
        return;
    }

    // Log d'information indiquant que le packet d'authentification est bien recu.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [NETWORK_IN] [AUTH] - Auth response packet received from server (size: %u bytes).",
        static_cast<unsigned>(event->packet->dataLength));

    // Crée un message destiné au thread simulation.
    NetworkINToSimulationMessage message{};

    // Spécifier le type de message pour que la simulation sache comment le traiter.
    message.type = NetworkINToSimulationMessageType::SERVER_AUTH_RESPONSE_PACKET_RELIABLE;
    // Copier le packet deserialise dans le message.
    message.authResponsePacket = authResponsePacket;

    // Envoie le message au thread simulation via la queue thread-safe.
    netToSimQueue.push(message);
}
