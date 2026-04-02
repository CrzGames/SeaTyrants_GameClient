#include "network/transport/incoming/events/disconnect.h"

#include <cstdint> // uint32_t, uint64_t
#include <RC2D/RC2D.h>

void ClientNetworkIncoming_Event_HandleDisconnect(
    const ENetEvent* event,
    NetworkState& networkState,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Vérifie que le peer associé à l'événement de déconnexion n'est pas nul.
    if (event->peer == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN,
                 "[CLIENT] [NETWORK_IN] [DISCONNECT] - Invalid disconnect event (event->peer == nullptr).");
        return;
    }

    // Supprime le référence au peer du serveur dans l'état réseau, car la connexion est maintenant fermée.
    networkState.peerServer = nullptr;

    // Crée un message destiné au thread simulation.
    NetworkINToSimulationMessage message{};

    // Spécifier le type de message pour que la simulation sache comment le traiter.
    message.type = NetworkINToSimulationMessageType::SERVER_EVENT_DISCONNECT;

    // Envoie le message au thread simulation via la queue thread-safe.
    netToSimQueue.push(message);

    // Log l'événement de déconnexion avec l'identifiant de connexion concerné.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [NETWORK_IN] [DISCONNECT] - Transport disconnected (server closed connection or timed out).");
}
