#include "network/transport/outgoing/queue_draining.h"

void ClientNetworkOutgoing_DrainSimulationToNetworkOutgoingQueue(
    SimulationToNetworkOUTQueue& simToNetQueue,
    std::deque<SimulationToNetworkOUTMessage>& simToNetOutMessages)
{
    // Drainer toute la queue simulation -> réseau sortant
    // dans la deque locale fournie en sortie.
    simToNetQueue.drain(simToNetOutMessages);
}