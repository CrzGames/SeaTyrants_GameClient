#pragma once

#include <array>   // std::array
#include <cstddef> // size_t
#include <cstdint> // uint8_t, uint64_t

#include <sodium.h> // crypto_sign_PUBLICKEYBYTES

// ============================================================================
// Secure-session metadata (client side)
// ============================================================================

// Taille (en bytes) du nonce transporte dans
// ClientSecureSessionHelloPacketReliable::clientNonce.
// Le serveur doit recopier exactement cette valeur dans
// ServerSecureSessionHelloResponsePacketReliable::clientNonceEcho.
static constexpr size_t CLIENT_SECURE_SESSION_CLIENT_NONCE_BYTES = 16;

// Domaine de signature Ed25519 applique par le serveur au payload secure-session.
// Le client doit reconstruire le message signe avec EXACTEMENT cette valeur.
static constexpr char CLIENT_SECURE_SESSION_SIGNING_DOMAIN[] =
    "SERVER_ED25519_DOMAIN_SECURE_SESSION_ATTESTATION_V1";

// Cle publique Ed25519 pinnee cote client pour l'environnement DEV.
static constexpr char CLIENT_SECURE_SESSION_SERVER_ED25519_PUBLIC_KEY_HEX_DEV[] =
    "af110dde7833e9aa3784e6b44af5754a67b659a09dd85877bdc0330c79e7e02f";

// Cle publique Ed25519 pinnee cote client pour l'environnement STAGING.
static constexpr char CLIENT_SECURE_SESSION_SERVER_ED25519_PUBLIC_KEY_HEX_STAGING[] =
    "8dd7caa479c6f3ee450605e2bb0d47fdcef9687314c5101f1ad03db54f28ac0c";

// Cle publique Ed25519 pinnee cote client pour l'environnement PRODUCTION.
static constexpr char CLIENT_SECURE_SESSION_SERVER_ED25519_PUBLIC_KEY_HEX_PRODUCTION[] =
    "8dd7caa479c6f3ee450605e2bb0d47fdcef9687314c5101f1ad03db54f28ac0c";

// Duree maximale attendue pour une attestation secure-session (en secondes).
// Le client peut refuser une attestation dont la fenetre de validite est
// anormalement longue.
static constexpr uint64_t CLIENT_SECURE_SESSION_SIGNATURE_TTL_SECONDS = 10;

// Initialise (une seule fois) la cle publique pinnee binaire a partir de
// la constante hardcodee de l'environnement courant (DEV/STAGING/PRODUCTION).
// Retourne false si la valeur HEX est invalide.
bool ClientSecureSession_InitializePinnedServerSigningPublicKey();

// Copie la cle publique pinnee binaire (32 bytes) dans outPublicKey.
// Retourne false si l'initialisation n'a pas encore ete faite.
bool ClientSecureSession_GetPinnedServerSigningPublicKey(
    std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>& outPublicKey);
