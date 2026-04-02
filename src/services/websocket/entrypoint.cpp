#include "services/websocket/entrypoint.h"

#include "core/context.h"
#include "core/threading/queues/simulation_to_websocket.h"
#include "services/websocket/process/simulation_dispatcher.h"

void ClientWebSocket_WaitAndProcessOneSimulationMessage_And_RunWebSocketLogic(void)
{
    // Recupere la reference vers la queue qui transporte les jobs
    // envoyes par le thread simulation vers le thread WebSocket.
    SimulationToWebSocketQueue& simulationToWebSocketQueue = GetSimulationToWebSocketQueue();

    // Declare l'objet qui recevra le prochain message a traiter.
    SimulationToWebSocketMessage simToWebSocketMessage;

    // Attend de facon bloquante qu'un message soit disponible dans la queue.
    // - Retourne true si un message a bien ete recupere.
    // - Retourne false si la queue a ete arretee (stop demande au shutdown).
    if (!simulationToWebSocketQueue.waitAndPop(simToWebSocketMessage))
    {
        // Si le wait s'arrete parce qu'on shutdown,
        // on quitte simplement cette iteration du thread WebSocket.
        return;
    }

    // Une fois le message recupere, on le transmet au dispatcher WebSocket.
    // Le dispatcher choisira le bon traitement selon message.type.
    ClientWebSocket_ProcessSimulationDispatcher(simToWebSocketMessage);
}
