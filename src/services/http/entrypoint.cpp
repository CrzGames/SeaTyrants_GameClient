#include "services/http/entrypoint.h"

#include "services/http/process/simulation_dispatcher.h"
#include "core/threading/queues/simulation_to_http.h"
#include "core/context.h"

void ClientHttp_WaitAndProcessOneSimulationMessage_And_RunHttpLogic(void)
{
    // Récupère la référence vers la queue qui transporte les jobs
    // envoyés par le thread simulation vers le thread HTTP.
    SimulationToHttpQueue& simulationToHttpQueue = GetSimulationToHttpQueue();

    // Déclare l'objet qui recevra le prochain message à traiter.
    SimulationToHttpMessage simulationToHttpMsg;

    // Attend de façon bloquante qu'un message soit disponible dans la queue.
    // - Retourne true si un message a bien été récupéré.
    // - Retourne false si la queue a été arrêtée (stop demandé au shutdown).
    if (!simulationToHttpQueue.waitAndPop(simulationToHttpMsg))
    {
        // Si le wait s'arrête parce qu'on shutdown,
        // on quitte simplement cette itération du thread HTTP.
        return;
    }

    // Une fois le message récupéré, on le transmet au dispatcher HTTP.
    // Le dispatcher choisira le bon traitement selon message.type.
    ClientHttp_ProcessSimulationDispatcher(simulationToHttpMsg);
}