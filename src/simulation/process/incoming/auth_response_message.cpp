#include "simulation/process/incoming/auth_response_message.h"

#include <RC2D/RC2D.h>
#include <mutex>

void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleAuthResponseMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage)
{
    // Verrouiller l'etat reseau partage (thread simulation + thread reseau).
    std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

    // Evaluer le statut metier du token:
    // - SUCCESS            => token accepte
    // - INVALID_AUTH_TOKEN => token refuse
    if (networkInToSimMessage.authResponsePacket.status == ServerAuthResponseStatus::SUCCESS)
    {
        networkState.authTokenValidated = true;
        RC2D_log(
            RC2D_LOG_INFO,
            "[CLIENT] [SIMULATION] [AUTH] - Auth response processed: token accepted.");
    }
    else
    {
        networkState.authTokenValidated = false;
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [SIMULATION] [AUTH] - Auth response processed: token rejected (status=%u).",
            static_cast<unsigned>(networkInToSimMessage.authResponsePacket.status));
    }
}
