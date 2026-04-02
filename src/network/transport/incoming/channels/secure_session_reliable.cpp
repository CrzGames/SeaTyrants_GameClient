#include "network/transport/incoming/channels/secure_session_reliable.h"

#include "network/serialization/deserialize_packets_server.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_Channel_SecureSessionReliable(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Tente de deserialiser la reponse secure-session du serveur.
    ServerSecureSessionHelloResponsePacketReliable secureSessionHelloResponsePacket{};
    if (!deserializeServerSecureSessionHelloResponsePacketReliable(
            event->packet->data,
            event->packet->dataLength,
            secureSessionHelloResponsePacket))
    {
        // Si la deserialisation echoue, le packet est invalide ou mal forme.
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_IN] [SECURE_SESSION] - Failed to deserialize secure-session hello response (size: %u bytes).",
            static_cast<unsigned>(event->packet->dataLength));
        return;
    }

    // Log d'information indiquant que le packet de secure-session est bien recu.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [NETWORK_IN] [SECURE_SESSION] - Secure-session hello response received from server (size: %u bytes).",
        static_cast<unsigned>(event->packet->dataLength));

    // Crée un message destiné au thread simulation.
    NetworkINToSimulationMessage message{};

    // Spécifier le type de message pour que la simulation sache comment le traiter.
    message.type = NetworkINToSimulationMessageType::SERVER_SECURE_SESSION_HELLO_RESPONSE_PACKET_RELIABLE;
    // Copier le packet deserialise dans le message.
    message.secureSessionHelloResponsePacket = secureSessionHelloResponsePacket;

    // Envoie le message au thread simulation via la queue thread-safe.
    netToSimQueue.push(message);
}
