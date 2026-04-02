#include "network/transport/incoming/dispatch_by_channel.h"

#include "network/channels/channel.h"
#include "network/transport/incoming/channels/secure_session_reliable.h"
#include "network/transport/incoming/channels/auth_reliable.h"
#include "network/transport/incoming/channels/game_reliable.h"
#include "network/transport/incoming/channels/game_unreliable.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_DispatchByChannel(const ENetEvent* event, NetworkINToSimulationQueue& netToSimQueue)
{
    // Récupère le channel sur lequel le packet est arrivé.
    const NetworkChannel channel = static_cast<NetworkChannel>(event->channelID);

    // Dispatch le traitement du packet selon le channel ENet utilisé.
    switch (channel)
    {
        case NetworkChannel::SECURE_SESSION_RELIABLE:
            ClientNetworkIncoming_Channel_SecureSessionReliable(event, netToSimQueue);
            break;

        case NetworkChannel::AUTH_RELIABLE:
            ClientNetworkIncoming_Channel_AuthReliable(event, netToSimQueue);
            break;

        case NetworkChannel::GAME_RELIABLE:
            ClientNetworkIncoming_Channel_GameReliable(event, netToSimQueue);
            break;

        case NetworkChannel::GAME_UNRELIABLE:
            ClientNetworkIncoming_Channel_GameUnreliable(event, netToSimQueue);
            break;

        default:
            RC2D_log(
                RC2D_LOG_WARN,
                "[CLIENT] [NETWORK_IN] [RECEIVE] Unknown channel=%u from server.",
                static_cast<unsigned>(channel));
            break;
    }
}
