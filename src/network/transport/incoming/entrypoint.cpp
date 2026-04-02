#include "network/transport/incoming/entrypoint.h"

#include "core/context.h"
#include "network/transport/incoming/events/connect.h"
#include "network/transport/incoming/events/disconnect.h"
#include "network/transport/incoming/events/receive.h"

#include <RC2D/RC2D.h>

void ClientNetworkIncoming_ProcessENetEvent(ENetHost* host, const ENetEvent* event)
{
    // Guards de base.
    if (host == nullptr || event == nullptr)
    {
        return;
    }

    // Récupère la référence vers l’état du réseau.
    NetworkState& networkState = GetNetworkState();

    // Récupère la référence vers la file d’attente des messages du réseau vers la simulation.
    NetworkINToSimulationQueue& netToSimQueue = GetNetworkINToSimulationQueue();

    if (event->type == ENET_EVENT_TYPE_CONNECT)
    {
        // Traite l’événement de connexion.
        ClientNetworkIncoming_Event_HandleConnect(event, networkState, netToSimQueue);
    }
    else if (event->type == ENET_EVENT_TYPE_DISCONNECT || event->type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT)
    {
        // Traite l’événement de déconnexion.
        ClientNetworkIncoming_Event_HandleDisconnect(event, networkState, netToSimQueue);
    }
    else if (event->type == ENET_EVENT_TYPE_RECEIVE)
    {
        // Traite l’événement de réception de packet.
        ClientNetworkIncoming_Event_HandleReceive(event, networkState, netToSimQueue);
    }
    else
    {
        RC2D_log(
            RC2D_LOG_DEBUG,
            "[CLIENT] [NETWORK_IN] Ignored ENet event type=%u on host=%p.",
            static_cast<unsigned>(event->type),
            host);
    }
}

