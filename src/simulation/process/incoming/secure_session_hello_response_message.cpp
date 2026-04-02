#include "simulation/process/incoming/secure_session_hello_response_message.h"

#include "core/context.h"
#include "crypto/kx.h"
#include "network/packets/client/reliable.h"
#include "network/protocol/secure_session.h"
#include "network/protocol/secure_session_attestation.h"
#include "network/serialization/serialize_packets_client.h"

#include <RC2D/RC2D.h> // RC2D_log

#include <array>   // std::array
#include <cstring> // std::memcmp
#include <ctime>   // std::time
#include <mutex>   // std::lock_guard, std::mutex

void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleSecureSessionHelloResponseMessage(
    NetworkState& networkState,
    const NetworkINToSimulationMessage& networkInToSimMessage)
{
    // Raccourci vers le packet de reponse secure-session recu du serveur.
    const ServerSecureSessionHelloResponsePacketReliable& packet = networkInToSimMessage.secureSessionHelloResponsePacket;

    // Verrouiller l'etat partage (simulation + reseau).
    std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

    // Si le serveur annonce un echec de handshake, on reset l'etat de session.
    if (packet.status != ServerSecureSessionHelloResponseStatus::SUCCESS)
    {
        networkState.secureSessionEstablished = false;
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [SIMULATION] [SECURE_SESSION] - Secure-session rejected by server (status=%u).",
            static_cast<unsigned>(packet.status));
        return;
    }

    // Verifier que le nonce echo renvoye par le serveur correspond au nonce
    // envoye dans le hello secure-session.
    if (std::memcmp(
            packet.clientNonceEcho.data(),
            networkState.pendingClientNonce.data(),
            networkState.pendingClientNonce.size()) != 0)
    {
        networkState.secureSessionEstablished = false;
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [SIMULATION] [SECURE_SESSION] - Nonce mismatch in secure-session hello response.");
        return;
    }

    // Verifier la signature Ed25519 de l'attestation secure-session.
    if (!ClientSecureSession_VerifyServerHelloResponseAttestation(packet))
    {
        networkState.secureSessionEstablished = false;
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [SIMULATION] [SECURE_SESSION] - Invalid server attestation signature.");
        return;
    }

    // Verifier la coherence de la fenetre temporelle de l'attestation.
    if (packet.expiresAtUnixSeconds < packet.issuedAtUnixSeconds)
    {
        networkState.secureSessionEstablished = false;
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [SIMULATION] [SECURE_SESSION] - Invalid attestation time window.");
        return;
    }

    const uint64_t validityWindow = packet.expiresAtUnixSeconds - packet.issuedAtUnixSeconds;
    if (validityWindow > CLIENT_SECURE_SESSION_SIGNATURE_TTL_SECONDS)
    {
        networkState.secureSessionEstablished = false;
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [SIMULATION] [SECURE_SESSION] - Attestation TTL too large (window=%llu).",
            static_cast<unsigned long long>(validityWindow));
        return;
    }

    // Refuser une attestation expiree.
    const uint64_t nowUnixSeconds = static_cast<uint64_t>(std::time(nullptr));
    if (nowUnixSeconds > packet.expiresAtUnixSeconds)
    {
        networkState.secureSessionEstablished = false;
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [SIMULATION] [SECURE_SESSION] - Attestation expired (now=%llu, exp=%llu).",
            static_cast<unsigned long long>(nowUnixSeconds),
            static_cast<unsigned long long>(packet.expiresAtUnixSeconds));
        return;
    }

    // Deriver les cles de session client (rx/tx) avec la cle publique serveur.
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> rxKey{};
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> txKey{};
    if (!ClientCryptoKx_ComputeSessionKeys(
            networkState.cryptoKxState,
            packet.serverPublicKey,
            rxKey,
            txKey))
    {
        networkState.secureSessionEstablished = false;
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [SIMULATION] [SECURE_SESSION] - Failed to derive session keys.");
        return;
    }

    // Enregistrer les cles de session pour le transport chiffre.
    networkState.clientRxKey = rxKey;
    networkState.clientTxKey = txKey;

    // Marquer la secure-session comme etablie et activer le chiffrement pour le peer serveur.
    networkState.secureSessionEstablished = true;

    // Log de succes.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [SIMULATION] [SECURE_SESSION] - Secure-session established (nonceEcho=ok, signature=ok, ttl=ok, keysDerived=ok, encryption=enabled, issuedAt=%llu, expiresAt=%llu).",
        static_cast<unsigned long long>(packet.issuedAtUnixSeconds),
        static_cast<unsigned long long>(packet.expiresAtUnixSeconds));

    // Si la session secure-session est etablie, on peut à présent
    // envoyer le token d'authentification du client pour que le serveur puisse verifier l'identite du client.
    // On envoie un message du thread simulation vers le thread réseau pour que ce dernier puisse envoyer le packet d'authentification correspondant.
    if (networkState.secureSessionEstablished)
    {
        
        // Créer le packet ClientAuthPacketReliable avec le token d'authentification du client.
        ClientAuthPacketReliable authPacket{};
        authPacket.header.type = ClientReliablePacketType::CLIENT_AUTH_PACKET_RELIABLE;
        authPacket.authToken = networkState.authToken;

        // Sérialiser le packet d'authentification et l'envoyer au thread réseau via la queue simulationToNetworkOUTQueue.
        SimulationToNetworkOUTMessage simToNetMessage{};
        simToNetMessage.type = SimulationToNetworkOUTMessageType::CLIENT_AUTH_PACKET_RELIABLE;
        simToNetMessage.serializedPacket = serializeClientAuthPacketReliable(authPacket);
        GetSimulationToNetworkOUTQueue().push(simToNetMessage);
    }
}
