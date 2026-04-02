#pragma once

#include <cstdint> // uint16_t, uint32_t, etc.
#include <string>  // std::string
#include <array>   // std::array

#include <sodium.h> // crypto_kx_PUBLICKEYBYTES, crypto_sign_BYTES

#include "network/protocol/secure_session.h" // secure-session signature metadata

// ======================================================================================
// ServerReliablePacketType
//
// Type de packet envoyé sur le channel reliable serveur -> client.
//
// Tous les packets reliable doivent commencer par ServerReliablePacketHeader
// pour permettre au client de dispatcher correctement.
// ======================================================================================
enum class ServerReliablePacketType : uint8_t
{
    SERVER_SECURE_SESSION_HELLO_RESPONSE_PACKET_RELIABLE = 0,
    SERVER_AUTH_RESPONSE_PACKET_RELIABLE = 1,
    SERVER_MATCH_INIT_PACKET_RELIABLE = 2,
    SERVER_WORLD_STATIC_STATE_INIT_PACKET_RELIABLE = 3,
    SERVER_MATCH_START_PACKET_RELIABLE = 4,
    SERVER_MATCH_END_PACKET_RELIABLE = 5,
};

struct ServerReliablePacketHeader
{
    ServerReliablePacketType type;
};

// ======================================================================================
// Liste des packets reliable envoyés par le serveur au client.
// ======================================================================================

struct ServerMatchInitPacketReliable
{
    // Header commun à tous les packets reliable serveur -> client.
    // Permet au client d'identifier le type de message reçu.
    ServerReliablePacketHeader header;

    // Nom de la map à charger côté client.
    // Le client doit posséder cette map localement (installée avec le jeu ou via un patch).
    std::string mapName;

    // Version de la map attendue par le serveur.
    // Permet de vérifier que le client possède exactement la même version
    // (évite les problèmes de désynchronisation ou certaines triches).
    uint32_t mapVersion;

    // Checksum de la map pour vérifier l'intégrité des données côté client.
    // Permet de détecter des maps modifiées ou corrompues.
    uint32_t mapChecksum;

    // Tick logique de simulation serveur auquel ce packet a été construit.
    // Sert de référence temporelle pour synchroniser la timeline client avec le serveur.
    uint64_t serverTick;

    // Fréquence de tick de la simulation serveur (ex: 128 Hz).
    // Permet au client de convertir les ticks serveur en temps réel.
    uint32_t serverTickRateHz;

    // Temps monotone du serveur en nanosecondes depuis le démarrage du moteur.
    // Utile pour estimer la latence et synchroniser l'horloge client avec celle du serveur.
    uint64_t serverTimeNs;
};

struct ServerWorldStaticStateInitPacketReliable
{
    // Header commun à tous les packets reliable serveur -> client.
    // Permet au client d'identifier le type de message reçu.
    ServerReliablePacketHeader header;

    // Plus tard :
    // seed
    // spawn points count
    // zones count
    // ou autres métadonnées statiques
};

struct ServerMatchStartPacketReliable
{
    // Header commun à tous les packets reliable serveur -> client.
    ServerReliablePacketHeader header;

    // Tick logique de simulation serveur auquel ce packet a été construit.
    // Sert de référence temporelle pour synchroniser la timeline client avec le serveur.
    uint64_t serverTick;

    // Tick de simulation auquel le match commence officiellement côté serveur.
    // Le client peut utiliser cette valeur pour lancer un compte à rebours synchronisé.
    uint64_t matchStartTick;

    // Durée du compte à rebours avant le début du match exprimée en ticks serveur.
    // Exemple : 128 ticks = 1 seconde si le serveur tourne à 128 Hz.
    uint32_t countdownTicks;

    // Temps monotone du serveur en nanosecondes depuis le démarrage du moteur.
    // Utile pour estimer la latence et synchroniser l'horloge client avec celle du serveur.
    uint64_t serverTimeNs;
};

enum class ServerSecureSessionHelloResponseStatus : uint8_t
{
    SUCCESS = 0,
    INVALID_CLIENT_KEY = 1,
    SERVER_ATTESTATION_FAILED = 2,
};

struct ServerSecureSessionHelloResponsePacketReliable
{
    // Header commun à tous les packets reliable serveur -> client.
    ServerReliablePacketHeader header;

    // Statut de la réponse du serveur
    ServerSecureSessionHelloResponseStatus status = ServerSecureSessionHelloResponseStatus::INVALID_CLIENT_KEY;

    // IMPORTANT:
    // Les 4 champs ci-dessous (serverPublicKey, issuedAtUnixSeconds,
    // expiresAtUnixSeconds, clientNonceEcho) sont copiés dans
    // ServerCryptoSigningSecureSessionPayload (voir crypto/signing.h),
    // puis signés côté serveur via
    // ServerCryptoSigning_SignSecureSessionPayload(...).

    // Clé publique KX (X25519) du serveur utilisée par le client dans
    // crypto_kx_client_session_keys(...) pour dériver les clés de session.
    std::array<uint8_t, crypto_kx_PUBLICKEYBYTES> serverPublicKey{};

    // Timestamp UNIX (secondes) d'émission de l'attestation signée.
    uint64_t issuedAtUnixSeconds = 0;

    // Timestamp UNIX (secondes) d'expiration de l'attestation signée.
    // Le client doit refuser la réponse si now > expiresAtUnixSeconds.
    uint64_t expiresAtUnixSeconds = 0;

    // Copie exacte de la valeur reçue dans
    // ClientSecureSessionHelloPacketReliable::clientNonce.
    // Le client doit vérifier que cette valeur == son nonce local envoyé.
    std::array<uint8_t, CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES> clientNonceEcho{};

    // Signature Ed25519 detached calculée côté serveur sur le payload:
    // [serverPublicKey, issuedAtUnixSeconds, expiresAtUnixSeconds, clientNonceEcho]
    // (avec le domaine de signature défini dans crypto/signing.cpp).
    // Vérification côté client via crypto_sign_verify_detached(...) avec
    // la clé publique Ed25519 serveur pinnée (hardcodée).
    std::array<uint8_t, crypto_sign_BYTES> signature{};
};

enum class ServerAuthResponseStatus : uint8_t
{
    SUCCESS = 0,
    INVALID_AUTH_TOKEN = 1,
};

struct ServerAuthResponsePacketReliable
{
    // Header commun à tous les packets reliable serveur -> client.
    ServerReliablePacketHeader header;

    // Statut de la réponse du serveur
    ServerAuthResponseStatus status;
};
