#pragma once

#include <array>       // std::array
#include <cstdint>     // uint16_t
#include <mutex>       // std::mutex
#include <string>      // std::string
#include <string_view> // std::string_view
#include <vector>      // std::vector

#include <rcenet/RCENET_enet.h> // ENetPeer
#include <sodium.h>             // crypto_kx_SESSIONKEYBYTES

#include "crypto/kx.h"          // ClientCryptoKxState
#include "network/protocol/secure_session.h" // CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES
#include "services/http/types/auth/responses.h" // AuthSignInHTTPServerEntry

struct NetworkState
{
    // ------------------------------------------------------------------------
    // Connections - ATTENTION: thread reseau UNIQUEMENT
    // ------------------------------------------------------------------------

    // ENetPeer* du serveur, initialement nullptr, valide apres connexion reussie.
    ENetPeer* peerServer = nullptr;

    // ------------------------------------------------------------------------
    // Session/Crypto - protegee par mutex (thread reseau + simulation)
    // ------------------------------------------------------------------------

    // Etat KX (X25519/libsodium crypto_kx):
    // - contient la cle KX client (publique/privee) utilisee pour derivation rx/tx
    // - generee au boot du client
    ClientCryptoKxState cryptoKxState{};

    // Vrai une fois la secure-session validee cote client
    // (packet server hello response avec status SUCCESS, nonce verifie, signature verifiee, et cles de session derivees).
    bool secureSessionEstablished = false;

    // Vrai uniquement si le serveur du jeu a valide le token d'authentification
    // (AuthResponse status == SUCCESS).
    bool authTokenValidated = false;

    // Bearer token opaque recupere apres le signin via le backend SeaTyrants.
    // Ce token provient du backend CrzGames et est ensuite envoye au serveur de jeu.
    std::string authToken = "";

    // Endpoint Quilkin recupere depuis la reponse signin du backend SeaTyrants.
    // C'est la cible preferentielle pour la connexion reseau du client.
    std::string quilkinDns = "";
    uint16_t quilkinPort = 0;

    // Nonce envoye dans le dernier hello secure-session.
    std::array<uint8_t, CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES> pendingClientNonce{};

    // Cles derivees cote client:
    // - clientTxKey: utilisee pour chiffrer les paquets sortants client->serveur
    // - clientRxKey: utilisee pour dechiffrer les paquets entrants serveur->client
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> clientTxKey{};
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> clientRxKey{};

    // Mutex de protection des champs crypto/session/auth utilises depuis plusieurs threads
    // (reseau + simulation + rendu/menu).
    std::mutex sessionCryptoMutex;

    // ------------------------------------------------------------------------
    // Etat partage du flow de connexion menu -> backend SeaTyrants
    // Protege lui aussi par `sessionCryptoMutex`.
    // ------------------------------------------------------------------------

    // Vrai quand une requete HTTP /signin a ete envoyee et qu'on attend encore
    // la reponse du backend SeaTyrants.
    bool authSignInRequestPending = false;

    // Vrai uniquement si la derniere reponse /signin est un succes metier.
    bool authSignInLastRequestSucceeded = false;

    // Permet au menu d'afficher l'overlay de selection des serveurs apres une
    // authentification HTTP reussie.
    bool authSignInServerSelectionVisible = false;

    // Code HTTP brut de la derniere reponse /signin recue par le client.
    long authSignInLastHttpStatusCode = 0;

    // Code metier renvoye par le backend quand present.
    std::string authSignInLastCode = "";

    // Message metier renvoye par le backend ou construit cote client.
    std::string authSignInLastMessage = "";

    // Liste minimale des serveurs a afficher dans le menu une fois connecte.
    std::vector<AuthSignInHTTPServerEntry> authSignInAvailableServers{};

    // --------------------------------------------------------------------------
    // API externe (backend web SeaTyrants utilise pour le signin HTTP)
    // --------------------------------------------------------------------------

#if GAME_ENV_DEV
    static constexpr std::string_view baseUrlApi = "http://localhost:3500";
#elif GAME_ENV_STAGING
    static constexpr std::string_view baseUrlApi = "https://staging.api.seatyrants.com";
#elif GAME_ENV_PRODUCTION
    static constexpr std::string_view baseUrlApi = "https://api.seatyrants.com";
#else
#error "Define one of GAME_ENV_DEV, GAME_ENV_STAGING or GAME_ENV_PRODUCTION"
#endif
};
