#include "network/protocol/secure_session_attestation.h"

#include "network/protocol/secure_session.h" // ClientSecureSession_GetPinnedServerSigningPublicKey

#include <RC2D/RC2D.h> // RC2D_log

#include <array>  // std::array
#include <vector> // std::vector

// Ecrit un uint64 dans le buffer en big-endian.
// Ce format doit rester strictement identique au serveur pour la signature.
static void ClientSecureSessionAttestation_WriteU64Be(
    std::vector<uint8_t>& out,
    uint64_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Construit le message canonique signe par le serveur:
// [domain][server_kx_pub][issued_at_be64][expires_at_be64][client_nonce_echo]
static void ClientSecureSessionAttestation_BuildSignedMessage(
    const ServerSecureSessionHelloResponsePacketReliable& packet,
    std::vector<uint8_t>& outMessage)
{
    outMessage.clear();
    outMessage.reserve(
        (sizeof(CLIENT_SECURE_SESSION_SIGNING_DOMAIN) - 1) +
        packet.serverPublicKey.size() +
        sizeof(uint64_t) +
        sizeof(uint64_t) +
        packet.clientNonceEcho.size());

    outMessage.insert(
        outMessage.end(),
        CLIENT_SECURE_SESSION_SIGNING_DOMAIN,
        CLIENT_SECURE_SESSION_SIGNING_DOMAIN + (sizeof(CLIENT_SECURE_SESSION_SIGNING_DOMAIN) - 1));

    outMessage.insert(
        outMessage.end(),
        packet.serverPublicKey.begin(),
        packet.serverPublicKey.end());

    ClientSecureSessionAttestation_WriteU64Be(outMessage, packet.issuedAtUnixSeconds);
    ClientSecureSessionAttestation_WriteU64Be(outMessage, packet.expiresAtUnixSeconds);

    outMessage.insert(
        outMessage.end(),
        packet.clientNonceEcho.begin(),
        packet.clientNonceEcho.end());
}

bool ClientSecureSession_VerifyServerHelloResponseAttestation(
    const ServerSecureSessionHelloResponsePacketReliable& packet)
{
    // Charger la cle publique de signature serveur pinnee cote client.
    std::array<uint8_t, crypto_sign_PUBLICKEYBYTES> pinnedServerSigningPublicKey{};
    if (!ClientSecureSession_GetPinnedServerSigningPublicKey(pinnedServerSigningPublicKey))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [SECURE_SESSION] Pinned server signing public key not initialized.");
        return false;
    }

    // Reconstruire le payload canonicalise qui a ete signe cote serveur.
    std::vector<uint8_t> message;
    ClientSecureSessionAttestation_BuildSignedMessage(packet, message);

    // Verifier la signature detached Ed25519.
    const int verifyResult = crypto_sign_verify_detached(
        packet.signature.data(),
        message.data(),
        static_cast<unsigned long long>(message.size()),
        pinnedServerSigningPublicKey.data());

    return verifyResult == 0;
}
