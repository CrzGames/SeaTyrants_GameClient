#include "simulation/queue_draining.h"

void ClientSimulation_DrainNetworkIncomingToSimulationMessages(
    NetworkINToSimulationQueue& netToSimQueue,
    std::deque<NetworkINToSimulationMessage>& outMessages)
{
    // Drainer toute la queue réseau -> simulation
    // dans la deque locale fournie en sortie.
    netToSimQueue.drain(outMessages);
}

void ClientSimulation_DrainHttpToSimulationMessages(
    HttpToSimulationQueue& httpToSimQueue,
    std::deque<HttpToSimulationMessage>& outMessages)
{
    // Drainer toute la queue HTTP -> simulation
    // dans la deque locale fournie en sortie.
    httpToSimQueue.drain(outMessages);
}

void ClientSimulation_DrainWebSocketToSimulationMessages(
    WebSocketToSimulationQueue& websocketToSimQueue,
    std::deque<WebSocketToSimulationMessage>& outMessages)
{
    // Drainer toute la queue WebSocket -> simulation
    // dans la deque locale fournie en sortie.
    websocketToSimQueue.drain(outMessages);
}
