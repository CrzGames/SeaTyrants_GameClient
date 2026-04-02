#include "network/transport/incoming/packets/snapshot_full.h"

#include "network/serialization/deserialize_packets_server.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_HandlePacket_SnapshotFull(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Tente de deserialiser le packet snapshot full envoye par le serveur.
    ServerSnapshotFullPacketUnreliable snapshotFullPacket{};
    if (!deserializeServerSnapshotFullPacketUnreliable(
            event->packet->data,
            event->packet->dataLength,
            snapshotFullPacket))
    {
        // Si la deserialisation echoue, le packet est invalide ou mal forme.
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_IN] [SNAPSHOT] Failed to deserialize snapshot full packet (size=%u).",
            static_cast<unsigned>(event->packet->dataLength));
        return;
    }

    // Crée un message destiné au thread simulation.
    NetworkINToSimulationMessage message{};

    // Spécifier le type de message pour que la simulation sache comment le traiter.
    message.type = NetworkINToSimulationMessageType::SERVER_SNAPSHOT_FULL_PACKET_UNRELIABLE;
    // Copier le packet deserialise dans le message.
    message.snapshotFullPacket = snapshotFullPacket;

    // Envoie le message au thread simulation via la queue thread-safe.
    netToSimQueue.push(message);
}

