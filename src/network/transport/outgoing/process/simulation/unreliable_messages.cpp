#include "network/transport/outgoing/process/simulation/unreliable_messages.h"

#include "network/transport/outgoing/enet_send_packets.h"

#include <RC2D/RC2D.h>

void ClientNetworkOutgoing_ProcessSimulationDispatcher_HandleUnreliableMessages(
    const NetworkState& networkState,
    bool hasInputUnreliable,
    const SimulationToNetworkOUTMessage& lastInputUnreliable,
    bool hasClockSyncUnreliable,
    const SimulationToNetworkOUTMessage& lastClockSyncUnreliable)
{
    // Cote client, la destination est toujours le peer serveur unique.
    ENetPeer* peer = networkState.peerServer;
    if (peer == nullptr)
    {
        // Pas de connexion active: aucun envoi possible.
        return;
    }

    // Si un message input unreliable est present, verifier son type puis envoyer.
    if (hasInputUnreliable)
    {
        if (lastInputUnreliable.type == SimulationToNetworkOUTMessageType::CLIENT_INPUT_PACKET_UNRELIABLE)
        {
            ClientNetworkOutgoing_SendInputPacketUnreliable(peer, lastInputUnreliable.serializedPacket);
        }
        else
        {
            // Protection defensive: le slot input ne doit contenir que ce type.
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [NETWORK_OUT] Unexpected unreliable message type=%u for input slot.",
                static_cast<unsigned>(lastInputUnreliable.type));
        }
    }

    // Si un message clock-sync unreliable est present, verifier son type puis envoyer.
    if (hasClockSyncUnreliable)
    {
        if (lastClockSyncUnreliable.type == SimulationToNetworkOUTMessageType::CLIENT_CLOCK_SYNC_PACKET_UNRELIABLE)
        {
            ClientNetworkOutgoing_SendClockSyncPacketUnreliable(peer, lastClockSyncUnreliable.serializedPacket);
        }
        else
        {
            // Protection defensive: le slot clock-sync ne doit contenir que ce type.
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [NETWORK_OUT] Unexpected unreliable message type=%u for clock-sync slot.",
                static_cast<unsigned>(lastClockSyncUnreliable.type));
        }
    }
}

