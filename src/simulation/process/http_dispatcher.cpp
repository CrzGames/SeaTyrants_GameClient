#include "simulation/process/http_dispatcher.h"

#include "simulation/process/http/auth_signupresponse_message.h"
#include "simulation/process/http/auth_signinresponse_message.h"

#include <RC2D/RC2D.h>

void ClientSimulation_ProcessHttpDispatcher(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const std::deque<HttpToSimulationMessage>& httpToSimulationMessages)
{
    // Parcourir tous les messages provenant du thread HTTP
    // qui ont été drainés pendant ce tick.
    for (std::deque<HttpToSimulationMessage>::const_iterator it = httpToSimulationMessages.begin();
         it != httpToSimulationMessages.end();
         ++it)
    {
        // Référence directe vers le message HTTP courant.
        const HttpToSimulationMessage& msg = *it;

        // Dispatch du traitement selon le type de message HTTP reçu.
        if (msg.type == HttpToSimulationMessageType::AUTH_SIGNUP_RESPONSE)
        {
            // Traiter la réponse du backend à notre requête d'inscription.
            ClientSimulation_ProcessHttpDispatcher_HandleAuthSignUpResponseMessage(
                networkState,
                simToNetQueue,
                msg);
        }
        else if (msg.type == HttpToSimulationMessageType::AUTH_SIGNIN_RESPONSE)
        {
            // Traiter la réponse du backend à notre requête de connexion.
            ClientSimulation_ProcessHttpDispatcher_HandleAuthSignInResponseMessage(
                networkState,
                simToNetQueue,
                msg);
        }
        else
        {
            RC2D_log(RC2D_LOG_ERROR, "[CLIENT] [SIMULATION] Unknown HttpToSimulationMessageType=%u", static_cast<uint8_t>(msg.type));
        }
    }
}
