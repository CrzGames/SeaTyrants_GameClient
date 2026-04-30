#include "simulation/process/http/auth_signinresponse_message.h"

#include <RC2D/RC2D.h>

#include <mutex>

void ClientSimulation_ProcessHttpDispatcher_HandleAuthSignInResponseMessage(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const HttpToSimulationMessage& httpToSimMessage)
{
    // Cette queue n'est volontairement plus utilisee ici.
    // Un signin HTTP reussi ne doit plus connecter automatiquement le client
    // au serveur du jeu: le menu doit d'abord afficher la liste des serveurs.
    (void)simToNetQueue;

    // Recuperer la reponse signin produite par le thread HTTP.
    const AuthSignInHTTPResponse& signInResponse = httpToSimMessage.authSignInResponse;

    {
        // Proteger les champs de session/auth partages avec les autres threads.
        std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

        // La requete HTTP vient de se terminer, qu'elle soit un succes ou un echec.
        networkState.authSignInRequestPending = false;

        // Memoriser les metadonnees de retour pour que le menu puisse afficher
        // un statut explicite au joueur.
        networkState.authSignInLastHttpStatusCode = signInResponse.httpStatusCode;
        networkState.authSignInLastCode = signInResponse.code;
        networkState.authSignInLastMessage = signInResponse.message;

        // Si le signin a echoue, nettoyer l'etat d'authentification local.
        if (!signInResponse.success)
        {
            // Effacer le bearer token memorise cote client.
            networkState.authToken.clear();

            // Marquer le token comme non valide cote serveur du jeu.
            networkState.authTokenValidated = false;

            // Effacer le DNS Quilkin memorise.
            networkState.quilkinDns.clear();

            // Reinitialiser le port Quilkin memorise.
            networkState.quilkinPort = 0;

            // Le dernier signin n'est pas considere comme valide.
            networkState.authSignInLastRequestSucceeded = false;

            // En cas d'echec, ne pas afficher l'overlay de selection serveur.
            networkState.authSignInServerSelectionVisible = false;

            // Vider la liste de serveurs issue d'un eventuel ancien signin.
            networkState.authSignInAvailableServers.clear();

            // Logger l'echec metier avec le maximum d'informations disponibles.
            RC2D_log(
                RC2D_LOG_WARN,
                "[CLIENT] [SIMULATION] [AUTH_SIGNIN] - Sign-in failed (httpStatus=%ld, code=%s, message=%s).",
                signInResponse.httpStatusCode,
                signInResponse.code.empty() ? "<none>" : signInResponse.code.c_str(),
                signInResponse.message.empty() ? "<none>" : signInResponse.message.c_str());

            // Sortir immediatement sans tentative de connexion reseau.
            return;
        }

        // Memoriser le bearer token CrzGames renvoye par le backend SeaTyrants.
        networkState.authToken = signInResponse.crzgamesTokenBearer;

        // Le serveur du jeu n'a pas encore valide le token a ce stade.
        networkState.authTokenValidated = false;

        // Memoriser le DNS Quilkin cible pour la connexion reseau.
        networkState.quilkinDns = signInResponse.quilkinDns;

        // Memoriser le port Quilkin cible pour la connexion reseau.
        networkState.quilkinPort = signInResponse.quilkinPort;

        // Signaler que la derniere reponse /signin est un succes exploitable.
        networkState.authSignInLastRequestSucceeded = true;

        // Autoriser le menu a afficher l'overlay de selection des serveurs.
        networkState.authSignInServerSelectionVisible = true;

        // Copier la liste des serveurs fournie par le backend pour l'UI menu.
        networkState.authSignInAvailableServers = signInResponse.listServers;
    }

    // Logger le succes signin et l'ouverture de la phase de selection serveur.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [SIMULATION] [AUTH_SIGNIN] - Sign-in succeeded (servers=%u, quilkin=%s:%u).",
        static_cast<unsigned>(signInResponse.listServers.size()),
        signInResponse.quilkinDns.c_str(),
        static_cast<unsigned>(signInResponse.quilkinPort));
}
