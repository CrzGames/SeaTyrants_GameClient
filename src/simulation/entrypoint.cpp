#include "simulation/entrypoint.h"

#include "core/context.h"
#include "simulation/queue_draining.h"
#include "simulation/process/network_incoming_dispatcher.h"
#include "simulation/process/http_dispatcher.h"
#include "simulation/process/websocket_dispatcher.h"

#include <chrono> // std::chrono::steady_clock, std::chrono::duration
#include <deque>  // std::deque

void ClientSimulation_DrainNetworkIncomingAndHttpAndWebSocketMessages_And_RunSimulationLogic(
    uint64_t currentTick,
    uint64_t dtNs,
    double dt)
{
    // Recuperer les references vers les queues inter-threads.
    NetworkINToSimulationQueue& networkInToSimulationQueue = GetNetworkINToSimulationQueue();
    SimulationToNetworkOUTQueue& simulationToNetworkOUTQueue = GetSimulationToNetworkOUTQueue();
    SimulationToHttpQueue& simulationToHttpQueue = GetSimulationToHttpQueue();
    HttpToSimulationQueue& httpToSimulationQueue = GetHttpToSimulationQueue();
    WebSocketToSimulationQueue& websocketToSimulationQueue = GetWebSocketToSimulationQueue();

    // Preparer la deque locale qui recevra les messages reseau entrants draines.
    std::deque<NetworkINToSimulationMessage> networkInToSimulationMessages;

    // Drainer la queue reseau -> simulation.
    ClientSimulation_DrainNetworkIncomingToSimulationMessages(
        networkInToSimulationQueue,
        networkInToSimulationMessages);

    // Preparer la deque locale qui recevra les messages HTTP entrants draines.
    std::deque<HttpToSimulationMessage> httpToSimulationMessages;

    // Drainer la queue HTTP -> simulation.
    ClientSimulation_DrainHttpToSimulationMessages(
        httpToSimulationQueue,
        httpToSimulationMessages);

    // Preparer la deque locale qui recevra les messages WebSocket entrants draines.
    std::deque<WebSocketToSimulationMessage> websocketToSimulationMessages;

    // Drainer la queue WebSocket -> simulation.
    ClientSimulation_DrainWebSocketToSimulationMessages(
        websocketToSimulationQueue,
        websocketToSimulationMessages);

    // Recuperer l'etat global du reseau.
    NetworkState& networkState = GetNetworkState();

    // Traiter tous les messages entrants provenant du thread reseau.
    ClientSimulation_ProcessNetworkIncomingDispatcher(
        networkState,
        simulationToNetworkOUTQueue,
        simulationToHttpQueue,
        networkInToSimulationMessages);

    // Traiter tous les messages entrants provenant du thread HTTP.
    ClientSimulation_ProcessHttpDispatcher(
        networkState,
        simulationToNetworkOUTQueue,
        httpToSimulationMessages);

    // Traiter tous les messages entrants provenant du thread WebSocket.
    ClientSimulation_ProcessWebSocketDispatcher(
        networkState,
        simulationToNetworkOUTQueue,
        websocketToSimulationMessages);

    (void)currentTick;
    (void)dtNs;
    (void)dt;

    // Simuler le monde gameplay pour le tick courant.
    /*ClientWorld_Simulate(
        gameState,
        currentTick,
        dtNs,
        dt);*/
}
