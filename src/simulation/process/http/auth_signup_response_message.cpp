#include "simulation/process/http/auth_signupresponse_message.h"

#include <RC2D/RC2D.h>

void ClientSimulation_ProcessHttpDispatcher_HandleAuthSignUpResponseMessage(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const HttpToSimulationMessage& httpToSimMessage)
{
    // Reponse backend issue du thread HTTP.
    const AuthSignUpHTTPResponse& signUpResponse = httpToSimMessage.authSignUpResponse;

    // Cette reponse ne modifie pas encore l'etat reseau/chiffrement.
    (void)networkState;
    (void)simToNetQueue;

    if (signUpResponse.success)
    {
        RC2D_log(
            RC2D_LOG_INFO,
            "[CLIENT] [SIMULATION] [AUTH_SIGNUP] - Sign-up succeeded (httpStatus=%ld, message=%s).",
            signUpResponse.httpStatusCode,
            signUpResponse.message.empty() ? "<none>" : signUpResponse.message.c_str());
    }
    else
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [SIMULATION] [AUTH_SIGNUP] - Sign-up failed (httpStatus=%ld, code=%s, message=%s).",
            signUpResponse.httpStatusCode,
            signUpResponse.code.empty() ? "<none>" : signUpResponse.code.c_str(),
            signUpResponse.message.empty() ? "<none>" : signUpResponse.message.c_str());
    }
}
