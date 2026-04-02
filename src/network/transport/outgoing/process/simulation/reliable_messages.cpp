#include "network/transport/outgoing/process/simulation/reliable_messages.h"

#include "network/transport/outgoing/enet_send_packets.h"

#include <RC2D/RC2D.h>

void ClientNetworkOutgoing_ProcessSimulationDispatcher_HandleReliableMessages(
    const NetworkState& networkState,
    const std::deque<SimulationToNetworkOUTMessage>& simulationToNetworkOUTReliableMessages)
{
    ENetPeer* peer = networkState.peerServer;
    if (peer == nullptr)
    {
        return;
    }

    for (std::deque<SimulationToNetworkOUTMessage>::const_iterator it = simulationToNetworkOUTReliableMessages.begin();
         it != simulationToNetworkOUTReliableMessages.end();
         ++it)
    {
        const SimulationToNetworkOUTMessage& msg = *it;

        if (msg.type == SimulationToNetworkOUTMessageType::CLIENT_SECURE_SESSION_HELLO_PACKET_RELIABLE)
        {
            ClientNetworkOutgoing_SendSecureSessionHelloPacketReliable(peer, msg.serializedPacket);
        }
        else if (msg.type == SimulationToNetworkOUTMessageType::CLIENT_AUTH_PACKET_RELIABLE)
        {
            ClientNetworkOutgoing_SendAuthPacketReliable(peer, msg.serializedPacket);
        }
        else if (msg.type == SimulationToNetworkOUTMessageType::CLIENT_READY_FOR_MATCH_PACKET_RELIABLE)
        {
            ClientNetworkOutgoing_SendReadyForMatchPacketReliable(peer, msg.serializedPacket);
        }
        else
        {
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [NETWORK_OUT] Unknown reliable SimulationToNetworkOUTMessageType=%u",
                static_cast<unsigned>(msg.type));
        }
    }
}
