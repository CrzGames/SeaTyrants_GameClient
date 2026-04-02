#include "simulation/process/http/auth_signinresponse_message.h"

#include <RC2D/RC2D.h>

#include <mutex>

void ClientSimulation_ProcessHttpDispatcher_HandleAuthSignInResponseMessage(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const HttpToSimulationMessage& httpToSimMessage)
{
    // Pas utilise pour ce flow:
    // le signin reussi declenche directement la connexion reseau.
    (void)simToNetQueue;

    // Reponse backend issue du thread HTTP.
    const AuthSignInHTTPResponse& signInResponse = httpToSimMessage.authSignInResponse;

    // Drapeau de connexion a declencher apres la mise a jour de l'etat.
    bool shouldConnectToGameServer = false;

    {
        // Proteger les champs session/crypto partages avec le thread reseau.
        std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

        // Cas echec sign-in: nettoyer l'etat d'auth local et logguer.
        if (!signInResponse.success)
        {
            networkState.authToken.clear();
            networkState.authTokenValidated = false;

            RC2D_log(
                RC2D_LOG_WARN,
                "[CLIENT] [SIMULATION] [AUTH_SIGNIN] - Sign-in failed (httpStatus=%ld, code=%s, message=%s).",
                signInResponse.httpStatusCode,
                signInResponse.code.empty() ? "<none>" : signInResponse.code.c_str(),
                signInResponse.message.empty() ? "<none>" : signInResponse.message.c_str());
            return;
        }

        // Cas succes sign-in: memoriser token + endpoint Quilkin renvoyes par le backend.
        networkState.authToken = signInResponse.tokenValue;
        networkState.authTokenValidated = false;
        networkState.quilkinDns = signInResponse.quilkinDns;
        networkState.quilkinPort = signInResponse.quilkinPort;

        shouldConnectToGameServer = true;
    }

    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [SIMULATION] [AUTH_SIGNIN] - Sign-in succeeded (tokenType=%s, tokenExpiresAt=%s, quilkin=%s:%u).",
        signInResponse.tokenType.c_str(),
        signInResponse.tokenExpiresAt.c_str(),
        signInResponse.quilkinDns.c_str(),
        static_cast<unsigned>(signInResponse.quilkinPort));

    // Si la connexion sign-in reussit, on peut se connecter au serveur de jeu via Quilkin.
    // En attendant on se connecte au serveur de jeu directement.
    if (shouldConnectToGameServer)
    {
        RC2D_log(
            RC2D_LOG_INFO,
            "[CLIENT] [SIMULATION] [AUTH_SIGNIN] - Connecting to game server (host=%s, port=%u).",
            networkState.peerServerAddress,
            static_cast<unsigned>(networkState.peerServerPort));
        rc2d_engine_networkConnectToServer(networkState.peerServerAddress, networkState.peerServerPort);
    }
}
