#include "network/transport/incoming/events/receive.h"

#include "network/transport/incoming/dispatch_by_channel.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_Event_HandleReceive(
    const ENetEvent* event,
    const NetworkState& networkState,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Protection basique sur pointeurs evenements.
    if (event == nullptr || event->peer == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "[CLIENT] [NETWORK_IN] [RECEIVE] Invalid receive event.");
        return;
    }

    // Le client n'accepte que des paquets du peer serveur courant.
    if (networkState.peerServer != nullptr && event->peer != networkState.peerServer)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_IN] [RECEIVE] Packet ignored from unknown peer.");
        return;
    }

    // Checker la validité du packet reçu.
    if (event->packet == nullptr || event->packet->dataLength == 0)
    {
        RC2D_log(RC2D_LOG_WARN, "[CLIENT] [NETWORK_IN] [RECEIVE] Received empty or null packet.");
        return;
    }

    // Dispatcher ensuite par channel applicatif.
    ClientNetworkIncoming_DispatchByChannel(event, netToSimQueue);
}

