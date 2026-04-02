#include "simulation/process/websocket_dispatcher.h"

#include <RC2D/RC2D.h>

#include <mutex>

void ClientSimulation_ProcessWebSocketDispatcher(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const std::deque<WebSocketToSimulationMessage>& websocketToSimulationMessages)
{
    // Ajouter si besoin
}

