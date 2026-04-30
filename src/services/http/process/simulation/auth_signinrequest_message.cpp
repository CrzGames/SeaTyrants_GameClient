#include "services/http/process/simulation/auth_signinrequest_message.h"

#include "core/context.h"
#include "services/http/requests/auth_signinrequest.h"
#include "services/http/types/auth/responses.h"

#include <RC2D/RC2D.h>

#include <exception>
#include <utility>

void ClientHttp_ProcessSimulationDispatcher_HandleAuthSignInRequestMessage(const SimulationToHttpMessage& simToHttpMessage)
{
    // Recuperer la queue de messages de http vers simulation pour pouvoir
    // envoyer la reponse a la simulation une fois la requete HTTP traitee.
    HttpToSimulationQueue& httpToSimulationQueue = GetHttpToSimulationQueue();

    // Ne jamais laisser une exception tuer le worker HTTP: convertir toute
    // erreur inattendue en reponse metier exploitable cote UI.
    AuthSignInHTTPResponse response{};
    try
    {
        response = ClientHttp_Auth_SignInRequest(simToHttpMessage.authSignInRequest);
    }
    catch (const std::exception& exception)
    {
        response.success = false;
        response.code = "E_BACKEND_CLIENT_EXCEPTION";
        response.message = "Le client a interrompu la tentative de connexion suite a une erreur interne.";

        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] Worker exception: %s",
            exception.what());
    }
    catch (...)
    {
        response.success = false;
        response.code = "E_BACKEND_CLIENT_EXCEPTION";
        response.message = "Le client a interrompu la tentative de connexion suite a une erreur interne.";

        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] Worker exception: <unknown>");
    }

    // Construire le message de retour de http vers simulation.
    HttpToSimulationMessage httpToSimulationMessage{};
    httpToSimulationMessage.type = HttpToSimulationMessageType::AUTH_SIGNIN_RESPONSE;
    httpToSimulationMessage.authSignInResponse = std::move(response);

    // Push vers simulation.
    httpToSimulationQueue.push(httpToSimulationMessage);
}
