#include "network/transport/incoming/events/connect.h"

#include <cstdint> // uintptr_t
#include <RC2D/RC2D.h>

void ClientNetworkIncoming_Event_HandleConnect(
    const ENetEvent* event,
    NetworkState& networkState,
    NetworkINToSimulationQueue& netToSimQueue)
{
    // Vérifie que le peer associé à l'événement de connexion n'est pas nul.
    if (event->peer == nullptr)
    {
        // Log un avertissement si l'événement de connexion est invalide (peer nul) et retourne sans faire d'autres traitements.
        RC2D_log(RC2D_LOG_WARN,
                 "[CLIENT] [NETWORK_IN] [CONNECT] - Invalid connect event (event->peer == nullptr).");
        return;
    }

    // Stocke une référence du peer du serveur dans l'état réseau pour pouvoir l'utiliser ultérieurement 
    // lors de l'envoi de messages au serveur.
    networkState.peerServer = event->peer;

    // Crée un message destiné au thread simulation.
    NetworkINToSimulationMessage message{};

    // Spécifier le type de message pour que la simulation sache comment le traiter.
    message.type = NetworkINToSimulationMessageType::SERVER_EVENT_CONNECT;

    // Envoie le message au thread simulation via la queue thread-safe.
    netToSimQueue.push(message);

    // Log des informations sur la connexion entrante, y compris l'adresse IP et le port du serveur. 
    // Si l'adresse IP ne peut pas être récupérée, affiche "<unknown>" à la place.
    char hostName[64] = {0};
    if (enet_address_get_host_ip(&event->peer->address, hostName, sizeof(hostName)) == 0)
    {
        RC2D_log(
            RC2D_LOG_INFO,
            "[CLIENT] [NETWORK_IN] [CONNECT] - Connected to server (address: %s).",
            hostName
        );
    }
    else
    {
        RC2D_log(
            RC2D_LOG_INFO,
            "[CLIENT] [NETWORK_IN] [CONNECT] - Connected to server (address: <unknown>)."
        );
    }
}
