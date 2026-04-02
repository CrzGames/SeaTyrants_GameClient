#include "simulation/process/incoming/disconnect_message.h"

#include <RC2D/RC2D.h>

#include <mutex>

void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleDisconnectMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage)
{
    // Proteger la reinitialisation de l'etat partage.
    std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

    // La secure-session n'est plus valide apres deconnexion.
    networkState.secureSessionEstablished = false;

    // Le token n'est plus considere valide apres deconnexion.
    networkState.authTokenValidated = false;

    RC2D_log(RC2D_LOG_INFO, "[CLIENT] [SIMULATION] [DISCONNECT] - Session state reset.");
}
