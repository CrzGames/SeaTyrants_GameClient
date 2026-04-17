#pragma once

#include "core/threading/queues/simulation_to_websocket.h"

void ClientWebSocket_ProcessSimulationDispatcher(const SimulationToWebSocketMessage& message);
