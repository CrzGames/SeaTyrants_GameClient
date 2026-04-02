#pragma once

#include <cstdint> // uint16_t, uint32_t, etc.
#include <string>  // std::string
#include <array>   // std::array

#include <sodium.h> // crypto_kx_PUBLICKEYBYTES

#include "network/protocol/secure_session.h" // CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES

// ======================================================================================
// ClientReliablePacketType
//
// Type de packet envoyé sur le channel reliable client -> serveur.
//
// Tous les packets reliable doivent commencer par ClientReliablePacketHeader
// pour permettre au serveur de dispatcher correctement.
// ======================================================================================
enum class ClientReliablePacketType : uint8_t
{
    CLIENT_SECURE_SESSION_HELLO_PACKET_RELIABLE = 0,
    CLIENT_AUTH_PACKET_RELIABLE = 1,
    CLIENT_READY_FOR_MATCH_PACKET_RELIABLE = 2,
};

struct ClientReliablePacketHeader
{
    ClientReliablePacketType type;
};

// ======================================================================================
// Liste des packets reliable envoyés par le client au serveur.
// ======================================================================================

struct ClientSecureSessionHelloPacketReliable
{
    // Header commun à tous les packets reliable client -> serveur
    ClientReliablePacketHeader header;

    // Version du protocole réseau utilisé par le client.
    // Permet au serveur de vérifier la compatibilité du protocole avant d'accepter la connexion.
    uint32_t networkProtocolVersion = 0;

    // Clé exchange publique du client pour établir une session sécurisée.
    // Utilisée par le serveur pour effectuer le key exchange et chiffrer les échanges suivants.
    std::array<uint8_t, crypto_kx_PUBLICKEYBYTES> clientPublicKey{};

    // Nonce anti-replay genere par le client pour CE handshake.
    // Le serveur doit renvoyer EXACTEMENT cette valeur dans
    // ServerSecureSessionHelloResponsePacketReliable::clientNonceEcho
    // avant verification de la signature cote client.
    std::array<uint8_t, CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES> clientNonce{};
};

struct ClientAuthPacketReliable
{
    // Header commun à tous les packets reliable client -> serveur
    ClientReliablePacketHeader header;

    // Token d’authentification (Bearer token oat de AdonisJS venant du backend d'authentification).
    std::string authToken;
};

struct ClientReadyForMatchPacketReliable
{
    // Header commun à tous les packets reliable client -> serveur.
    ClientReliablePacketHeader header;
};
