#include "services/http/process/simulation_dispatcher.h"

#include "services/http/process/simulation/auth_signinrequest_message.h"

#include <RC2D/RC2D.h>

void ClientHttp_ProcessSimulationDispatcher(const SimulationToHttpMessage& simToHttpMessage)
{
    switch (simToHttpMessage.type)
    {
        case SimulationToHttpMessageType::AUTH_SIGNIN_REQUEST:
            ClientHttp_ProcessSimulationDispatcher_HandleAuthSignInRequestMessage(simToHttpMessage);
            break;

        default:
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [HTTP] Unknown SimulationToHttpMessageType=%u",
                static_cast<uint8_t>(simToHttpMessage.type));
            break;
    }
}
