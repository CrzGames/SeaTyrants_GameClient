#include "network/transport/outgoing/enet_send_packets.h"

#include "network/channels/channel.h"

#include <RC2D/RC2D.h>

// Envoie un payload deja serialise vers un channel ENet donne.
// Retourne true si le packet est correctement queue cote ENet.
static bool ClientNetworkOutgoing_SendPacket(
    ENetPeer* peer,
    NetworkChannel channel,
    const std::vector<uint8_t>& bytes,
    enet_uint32 flags)
{
    // Guard: sans peer de destination, aucun envoi possible.
    if (peer == nullptr)
    {
        return false;
    }

    // Warning non bloquant si le payload depasse la cible "ultra-safe" UDP.
    // ENet peut fragmenter, mais on log pour visibilite.
    if (bytes.size() > kClientNetworkOutgoingPayloadMaxBytes)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_OUT] Payload size=%llu exceeds target max=%u (channel=%u).",
            static_cast<unsigned long long>(bytes.size()),
            static_cast<unsigned>(kClientNetworkOutgoingPayloadMaxBytes),
            static_cast<unsigned>(channel));
    }

    // Demander a ENet de creer un packet avec le payload serialise.
    ENetPacket* enetPacket = enet_packet_create(bytes.data(), bytes.size(), flags);
    if (enetPacket == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "[CLIENT] [NETWORK_OUT] Failed to create ENet packet.");
        return false;
    }

    // Queue le packet sur le peer et le channel cibles.
    // Si enet_peer_send echoue, le caller garde la responsabilite de destruction.
    if (enet_peer_send(peer, static_cast<enet_uint8>(channel), enetPacket) < 0)
    {
        enet_packet_destroy(enetPacket);
        RC2D_log(RC2D_LOG_ERROR, "[CLIENT] [NETWORK_OUT] Failed to queue ENet packet.");
        return false;
    }

    // Succes: ENet prend ownership du packet queue.
    return true;
}

bool ClientNetworkOutgoing_SendSecureSessionHelloPacketReliable(
    ENetPeer* peer,
    const std::vector<uint8_t>& bytes)
{
    return ClientNetworkOutgoing_SendPacket(
        peer,
        NetworkChannel::SECURE_SESSION_RELIABLE,
        bytes,
        ENET_PACKET_FLAG_RELIABLE);
}

bool ClientNetworkOutgoing_SendAuthPacketReliable(
    ENetPeer* peer,
    const std::vector<uint8_t>& bytes)
{
    return ClientNetworkOutgoing_SendPacket(
        peer,
        NetworkChannel::AUTH_RELIABLE,
        bytes,
        ENET_PACKET_FLAG_RELIABLE);
}

bool ClientNetworkOutgoing_SendReadyForMatchPacketReliable(
    ENetPeer* peer,
    const std::vector<uint8_t>& bytes)
{
    return ClientNetworkOutgoing_SendPacket(
        peer,
        NetworkChannel::GAME_RELIABLE,
        bytes,
        ENET_PACKET_FLAG_RELIABLE);
}

bool ClientNetworkOutgoing_SendInputPacketUnreliable(
    ENetPeer* peer,
    const std::vector<uint8_t>& bytes)
{
    return ClientNetworkOutgoing_SendPacket(
        peer,
        NetworkChannel::GAME_UNRELIABLE,
        bytes,
        0);
}

bool ClientNetworkOutgoing_SendClockSyncPacketUnreliable(
    ENetPeer* peer,
    const std::vector<uint8_t>& bytes)
{
    return ClientNetworkOutgoing_SendPacket(
        peer,
        NetworkChannel::GAME_UNRELIABLE,
        bytes,
        0);
}

