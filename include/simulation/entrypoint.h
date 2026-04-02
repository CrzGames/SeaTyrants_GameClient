#pragma once

#include <cstdint> // uint64_t

void ClientSimulation_DrainNetworkIncomingAndHttpAndWebSocketMessages_And_RunSimulationLogic(uint64_t currentTick, uint64_t dtNs, double dt);