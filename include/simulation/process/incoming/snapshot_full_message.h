#pragma once

#include "network/state.h"
#include "core/threading/queues/network_incoming_to_simulation.h"

void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleSnapshotFullMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage);
