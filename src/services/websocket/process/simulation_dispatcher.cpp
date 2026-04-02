#include "services/websocket/process/simulation_dispatcher.h"

#include <RC2D/RC2D.h>

void ClientWebSocket_ProcessSimulationDispatcher(const SimulationToWebSocketMessage& simToWebSocketMessage)
{
    // Aucun message simulation->websocket defini pour le moment.
    RC2D_log(
        RC2D_LOG_ERROR,
        "[CLIENT] [WEBSOCKET] - Received unsupported SimulationToWebSocketMessageType: %u",
        static_cast<unsigned>(simToWebSocketMessage.type));
}
