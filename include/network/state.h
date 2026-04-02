#pragma once

#include <array>       // std::array
#include <cstdint>     // uint16_t
#include <mutex>       // std::mutex
#include <string>      // std::string
#include <string_view> // std::string_view

#include <rcenet/RCENET_enet.h> // ENetPeer
#include <sodium.h>             // crypto_kx_SESSIONKEYBYTES

#include "crypto/kx.h"          // ClientCryptoKxState
#include "network/protocol/secure_session.h" // CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES

struct NetworkState
{
    // ------------------------------------------------------------------------
    // Connections - ATTENTION: thread reseau UNIQUEMENT
    // ------------------------------------------------------------------------

    // ENetPeer* du serveur, initialement nullptr, valide apres connexion reussie.
    ENetPeer* peerServer = nullptr;

    // Adresse du serveur de jeu, recuperee depuis la reponse sign-in du backend d'authentification.
    const char* peerServerAddress = "localhost";

    // Port du serveur de jeu, recuperee depuis la reponse sign-in du backend d'authentification.
    uint16_t peerServerPort = 12345;

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

    // Token d'authentification récupérer après le signin.
    // au près du backend web d'authentification.
    std::string authToken = "";

    // Endpoint Quilkin recupere depuis la reponse sign-in backend.
    // Utilisable plus tard pour la connexion reseau du client.
    std::string quilkinDns = "";
    uint16_t quilkinPort = 0;

    // Nonce envoye dans le dernier hello secure-session.
    std::array<uint8_t, CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES> pendingClientNonce{};

    // Cles derivees cote client:
    // - clientTxKey: utilisee pour chiffrer les paquets sortants client->serveur
    // - clientRxKey: utilisee pour dechiffrer les paquets entrants serveur->client
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> clientTxKey{};
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> clientRxKey{};

    // Mutex de protection des champs crypto/session utilises depuis plusieurs threads (reseau + simulation).
    std::mutex sessionCryptoMutex;


    // --------------------------------------------------------------------------
    // API externe (backend web d'authentification, pour signup/signin)
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
