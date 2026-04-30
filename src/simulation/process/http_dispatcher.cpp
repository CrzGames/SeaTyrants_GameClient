#include "simulation/process/http_dispatcher.h"

#include "simulation/process/http/auth_signinresponse_message.h"

#include <RC2D/RC2D.h>

void ClientSimulation_ProcessHttpDispatcher(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const std::deque<HttpToSimulationMessage>& httpToSimulationMessages)
{
    // Parcourir tous les messages provenant du thread HTTP
    // qui ont ete draines pendant ce tick.
    for (std::deque<HttpToSimulationMessage>::const_iterator it = httpToSimulationMessages.begin();
         it != httpToSimulationMessages.end();
         ++it)
    {
        // Reference directe vers le message HTTP courant.
        const HttpToSimulationMessage& msg = *it;

        if (msg.type == HttpToSimulationMessageType::AUTH_SIGNIN_RESPONSE)
        {
            // Traiter la reponse du backend a notre requete de connexion.
            ClientSimulation_ProcessHttpDispatcher_HandleAuthSignInResponseMessage(
                networkState,
                simToNetQueue,
                msg);
        }
        else
        {
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [SIMULATION] Unknown HttpToSimulationMessageType=%u",
                static_cast<uint8_t>(msg.type));
        }
    }
}
