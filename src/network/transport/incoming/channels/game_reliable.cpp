#include "network/transport/incoming/channels/game_reliable.h"

#include "network/transport/incoming/packet_type_reader.h"
#include "network/transport/incoming/packets/match_end.h"
#include "network/transport/incoming/packets/match_init.h"
#include "network/transport/incoming/packets/match_start.h"
#include "network/transport/incoming/packets/world_static_state_init.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_Channel_GameReliable(
    const ENetEvent* event,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Lire le type de packet reliable serveur en tete de payload.
    ServerReliablePacketType packetType{};
    if (!ClientNetworkIncoming_ReadServerReliablePacketType(event, packetType))
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_IN] [RELIABLE] Failed to read packet type.");
        return;
    }

    // Dispatcher vers le handler specialise selon le type de packet.
    switch (packetType)
    {
        case ServerReliablePacketType::SERVER_MATCH_INIT_PACKET_RELIABLE:
            ClientNetworkIncoming_HandlePacket_MatchInit(event, netToSimQueue);
            break;

        case ServerReliablePacketType::SERVER_WORLD_STATIC_STATE_INIT_PACKET_RELIABLE:
            ClientNetworkIncoming_HandlePacket_WorldStaticStateInit(event, netToSimQueue);
            break;

        case ServerReliablePacketType::SERVER_MATCH_START_PACKET_RELIABLE:
            ClientNetworkIncoming_HandlePacket_MatchStart(event, netToSimQueue);
            break;

        case ServerReliablePacketType::SERVER_MATCH_END_PACKET_RELIABLE:
            ClientNetworkIncoming_HandlePacket_MatchEnd(event, netToSimQueue);
            break;

        default:
            RC2D_log(
                RC2D_LOG_WARN,
                "[CLIENT] [NETWORK_IN] [RELIABLE] Unknown packet type=%u from server.",
                static_cast<unsigned>(packetType));
            break;
    }
}

