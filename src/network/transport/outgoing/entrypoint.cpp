#include "network/transport/outgoing/entrypoint.h"

#include "core/context.h"
#include "network/transport/outgoing/message_preparation.h"
#include "network/transport/outgoing/queue_draining.h"
#include "network/transport/outgoing/process/simulation_dispatcher.h"

#include <deque> // std::deque

void ClientNetworkOutgoing_DrainSimulationMessages_And_RunOutgoingNetworkLogic(ENetHost* host)
{
    // Vérifier que l'host ENet est valide avant toute opération.
    if (host == nullptr)
    {
        return;
    }

    // Récupérer une référence vers la queue simulation -> réseau sortant.
    SimulationToNetworkOUTQueue& simToNetQueue = GetSimulationToNetworkOUTQueue();

    // Récupérer l'état réseau global du serveur.
    NetworkState& networkState = GetNetworkState();

    // Préparer la deque locale qui recevra tous les messages drainés.
    std::deque<SimulationToNetworkOUTMessage> simToNetOutMessages;

    // Drainer la queue simulation -> réseau sortant.
    ClientNetworkOutgoing_DrainSimulationToNetworkOutgoingQueue(
        simToNetQueue,
        simToNetOutMessages);

    // Préparer la structure qui recevra les messages sortants déjà classés et coalescés par famille.
    ClientNetworkOutgoingPreparedMessages preparedMessages{};

    // Classer les messages sortants en :
    // - reliable conservés dans l'ordre
    // - le dernier message input unreliable
    // - le dernier message clock sync unreliable
    ClientNetworkOutgoing_SplitReliableAndCoalesceUnreliableMessages(
        simToNetOutMessages,
        preparedMessages);

    // Dispatcher le traitement des messages issus de la simulation.
    ClientNetworkOutgoing_ProcessSimulationDispatcher(
        networkState,
        preparedMessages);

    // Forcer le flush ENet pour limiter la latence d'envoi.
    enet_host_flush(host);
}
