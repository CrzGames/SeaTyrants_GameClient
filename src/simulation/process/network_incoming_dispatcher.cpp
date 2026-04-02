#include "simulation/process/network_incoming_dispatcher.h"

#include "simulation/process/incoming/auth_response_message.h"
#include "simulation/process/incoming/connect_message.h"
#include "simulation/process/incoming/disconnect_message.h"
#include "simulation/process/incoming/secure_session_hello_response_message.h"
#include "simulation/process/incoming/snapshot_full_message.h"

#include <RC2D/RC2D.h>

void ClientSimulation_ProcessNetworkIncomingDispatcher(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    SimulationToHttpQueue& simToHttpQueue,
    const std::deque<NetworkINToSimulationMessage>& networkInToSimulationMessages)
{
    // Parcourir tous les messages reseau entrants draines pendant ce tick.
    for (std::deque<NetworkINToSimulationMessage>::const_iterator it = networkInToSimulationMessages.begin();
         it != networkInToSimulationMessages.end();
         ++it)
    {
        const NetworkINToSimulationMessage& msg = *it;

        if (msg.type == NetworkINToSimulationMessageType::SERVER_EVENT_CONNECT)
        {
            ClientSimulation_ProcessNetworkIncomingDispatcher_HandleConnectMessage(
                networkState,
                simToNetQueue,
                msg);
        }
        else if (msg.type == NetworkINToSimulationMessageType::SERVER_EVENT_DISCONNECT)
        {
            ClientSimulation_ProcessNetworkIncomingDispatcher_HandleDisconnectMessage(
                networkState,
                msg);
        }
        else if (msg.type == NetworkINToSimulationMessageType::SERVER_SECURE_SESSION_HELLO_RESPONSE_PACKET_RELIABLE)
        {
            ClientSimulation_ProcessNetworkIncomingDispatcher_HandleSecureSessionHelloResponseMessage(
                networkState,
                msg);
        }
        else if (msg.type == NetworkINToSimulationMessageType::SERVER_AUTH_RESPONSE_PACKET_RELIABLE)
        {
            ClientSimulation_ProcessNetworkIncomingDispatcher_HandleAuthResponseMessage(
                networkState,
                msg);
        }
        else if (msg.type == NetworkINToSimulationMessageType::SERVER_SNAPSHOT_FULL_PACKET_UNRELIABLE)
        {
            ClientSimulation_ProcessNetworkIncomingDispatcher_HandleSnapshotFullMessage(
                networkState,
                msg);
        }
        else
        {
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [SIMULATION] Unknown NetworkINToSimulationMessageType=%u",
                static_cast<unsigned>(msg.type));
        }
    }
}

