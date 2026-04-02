#pragma once

#include <array>   // std::array
#include <cstdint> // uint8_t

#include <sodium.h> // crypto_kx_*

// Etat crypto KX du client.
// Le client conserve une paire de cles X25519 (public/secret) utilisee
// pour deriver les cles de session avec la cle publique KX du serveur.
struct ClientCryptoKxState
{
    std::array<uint8_t, crypto_kx_PUBLICKEYBYTES> clientPublicKey{};
    std::array<uint8_t, crypto_kx_SECRETKEYBYTES> clientSecretKey{};
};

// Initialise l'etat KX client:
// - genere une paire de cles KX client.
bool ClientCryptoKx_Initialize(ClientCryptoKxState& state);

// Retourne la cle publique KX client a envoyer dans le secure-session hello.
const std::array<uint8_t, crypto_kx_PUBLICKEYBYTES>& ClientCryptoKx_GetClientPublicKey(
    const ClientCryptoKxState& state);

// Derive les cles de session client a partir de:
// - la paire de cles KX client,
// - la cle publique KX serveur recue dans la reponse secure-session.
//
// outClientRxKey: cle utilisee pour dechiffrer serveur -> client.
// outClientTxKey: cle utilisee pour chiffrer client -> serveur.
bool ClientCryptoKx_ComputeSessionKeys(
    const ClientCryptoKxState& state,
    const std::array<uint8_t, crypto_kx_PUBLICKEYBYTES>& serverPublicKey,
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES>& outClientRxKey,
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES>& outClientTxKey);