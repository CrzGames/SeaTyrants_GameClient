#include "services/http/process/simulation/auth_signinrequest_message.h"

#include "services/http/types/auth/responses.h"
#include "core/context.h"
#include "services/http/requests/auth_signinrequest.h"

void ClientHttp_ProcessSimulationDispatcher_HandleAuthSignInRequestMessage(const SimulationToHttpMessage& simToHttpMessage)
{
    // Récupérer la queue de messages de http vers simulation pour pouvoir 
    // envoyer la réponse à la simulation une fois la requête HTTP traitée
    HttpToSimulationQueue& httpToSimulationQueue = GetHttpToSimulationQueue();

    // 1) faire la requête HTTP
    AuthSignInHTTPResponse response = ClientHttp_Auth_SignInRequest(simToHttpMessage.authSignInRequest);

    // 2) construire le message de retour de http vers simulation
    HttpToSimulationMessage httpToSimulationMessage{};
    httpToSimulationMessage.type = HttpToSimulationMessageType::AUTH_SIGNIN_RESPONSE;
    httpToSimulationMessage.authSignInResponse = std::move(response);

    // 3) push vers simulation
    httpToSimulationQueue.push(httpToSimulationMessage);
}
