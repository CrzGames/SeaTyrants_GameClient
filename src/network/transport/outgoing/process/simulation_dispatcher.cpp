#include "network/transport/outgoing/process/simulation_dispatcher.h"

#include "network/transport/outgoing/process/simulation/reliable_messages.h"
#include "network/transport/outgoing/process/simulation/unreliable_messages.h"

void ClientNetworkOutgoing_ProcessSimulationDispatcher(
    NetworkState& networkState,
    const ClientNetworkOutgoingPreparedMessages& preparedMessages)
{
    // Traiter d'abord les messages reliable.
    ClientNetworkOutgoing_ProcessSimulationDispatcher_HandleReliableMessages(
        networkState,
        preparedMessages.reliableMessages);

    // Traiter ensuite les messages unreliable.
    ClientNetworkOutgoing_ProcessSimulationDispatcher_HandleUnreliableMessages(
        networkState,
        preparedMessages.hasInputUnreliable,
        preparedMessages.lastInputUnreliable,
        preparedMessages.hasClockSyncUnreliable,
        preparedMessages.lastClockSyncUnreliable);
}
