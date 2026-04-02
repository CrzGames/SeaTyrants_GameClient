#include "network/transport/incoming/channels/game_unreliable.h"

#include "network/transport/incoming/packet_type_reader.h"
#include "network/transport/incoming/packets/snapshot_full.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_Channel_GameUnreliable(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Lire le type de packet unreliable serveur en tete de payload.
    ServerUnreliablePacketType packetType{};
    if (!ClientNetworkIncoming_ReadServerUnreliablePacketType(event, packetType))
    {
        RC2D_log(RC2D_LOG_WARN, "[CLIENT] [NETWORK_IN] [UNRELIABLE] Failed to read packet type.");
        return;
    }

    // Dispatcher vers le handler specialise selon le type de packet.
    switch (packetType)
    {
        case ServerUnreliablePacketType::SERVER_SNAPSHOT_FULL_PACKET_UNRELIABLE:
            ClientNetworkIncoming_HandlePacket_SnapshotFull(event, netToSimQueue);
            break;

        default:
            RC2D_log(
                RC2D_LOG_WARN,
                "[CLIENT] [NETWORK_IN] [UNRELIABLE] Unknown packet type=%u from server.",
                static_cast<unsigned>(packetType));
            break;
    }
}
