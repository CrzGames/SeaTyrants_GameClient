#pragma once

#include "core/threading/queues/simulation_to_http.h"

void ClientHttp_ProcessSimulationDispatcher(const SimulationToHttpMessage& message);
