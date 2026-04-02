#include "simulation/process/incoming/connect_message.h"

#include "crypto/kx.h"
#include "network/protocol/version.h"
#include "network/serialization/serialize_packets_client.h"

#include <RC2D/RC2D.h>
#include <sodium.h>

#include <mutex>

void ClientSimulation_ProcessNetworkIncomingDispatcher_HandleConnectMessage(
    NetworkState& networkState,
    SimulationToNetworkOUTQueue& simToNetQueue,
    const NetworkINToSimulationMessage& networkInToSimMessage)
{
    // Construire le packet secure-session hello envoye au serveur.
    ClientSecureSessionHelloPacketReliable secureSessionHelloPacket{};

    // Renseigner le type du packet reliable client.
    secureSessionHelloPacket.header.type = ClientReliablePacketType::CLIENT_SECURE_SESSION_HELLO_PACKET_RELIABLE;

    // Envoyer la version protocolaire client pour verification serveur.
    secureSessionHelloPacket.networkProtocolVersion = CLIENT_NETWORK_PROTOCOL_VERSION;

    // Copier la cle publique KX client dans le packet.
    secureSessionHelloPacket.clientPublicKey = ClientCryptoKx_GetClientPublicKey(networkState.cryptoKxState);

    // Generer un nonce aleatoire challenge/echo pour lier la reponse serveur.
    randombytes_buf(secureSessionHelloPacket.clientNonce.data(), secureSessionHelloPacket.clientNonce.size());

    {
        // Reinitialiser l'etat de session avant de lancer un nouveau handshake.
        std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

        // Memoriser le nonce envoye pour valider clientNonceEcho ensuite.
        networkState.pendingClientNonce = secureSessionHelloPacket.clientNonce;
    }

    // Construire le message sortant simulation -> reseau.
    SimulationToNetworkOUTMessage outMessage{};

    // Type de message: secure-session hello reliable.
    outMessage.type = SimulationToNetworkOUTMessageType::CLIENT_SECURE_SESSION_HELLO_PACKET_RELIABLE;

    // Serialiser le packet binaire a envoyer via ENet.
    outMessage.serializedPacket = serializeClientSecureSessionHelloPacketReliable(secureSessionHelloPacket);

    // Pousser le message dans la queue du thread reseau sortant.
    simToNetQueue.push(outMessage);

    // Log.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [SIMULATION] [CONNECT] - Secure-session hello packet queued for server.");
}

