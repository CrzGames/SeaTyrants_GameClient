#include "network/serialization/deserialize_packets_server.h"

#include "network/serialization/byte_reader.h"

bool deserializeServerSecureSessionHelloResponsePacketReliable(
    const void* data,
    size_t size,
    ServerSecureSessionHelloResponsePacketReliable& outPacket)
{
    ByteReader reader(data, size);

    uint8_t rawType = 0;
    if (!reader.readU8(rawType))
    {
        return false;
    }

    outPacket.header.type = static_cast<ServerReliablePacketType>(rawType);
    if (outPacket.header.type != ServerReliablePacketType::SERVER_SECURE_SESSION_HELLO_RESPONSE_PACKET_RELIABLE)
    {
        return false;
    }

    uint8_t rawStatus = 0;
    if (!reader.readU8(rawStatus))
    {
        return false;
    }
    outPacket.status = static_cast<ServerSecureSessionHelloResponseStatus>(rawStatus);

    if (!reader.readBytes(outPacket.serverPublicKey.data(), outPacket.serverPublicKey.size()))
    {
        return false;
    }

    if (!reader.readU64(outPacket.issuedAtUnixSeconds))
    {
        return false;
    }

    if (!reader.readU64(outPacket.expiresAtUnixSeconds))
    {
        return false;
    }

    if (!reader.readBytes(outPacket.clientNonceEcho.data(), outPacket.clientNonceEcho.size()))
    {
        return false;
    }

    if (!reader.readBytes(outPacket.signature.data(), outPacket.signature.size()))
    {
        return false;
    }

    return reader.empty();
}

bool deserializeServerAuthResponsePacketReliable(
    const void* data,
    size_t size,
    ServerAuthResponsePacketReliable& outPacket)
{
    ByteReader reader(data, size);

    uint8_t rawType = 0;
    if (!reader.readU8(rawType))
    {
        return false;
    }

    outPacket.header.type = static_cast<ServerReliablePacketType>(rawType);
    if (outPacket.header.type != ServerReliablePacketType::SERVER_AUTH_RESPONSE_PACKET_RELIABLE)
    {
        return false;
    }

    uint8_t rawStatus = 0;
    if (!reader.readU8(rawStatus))
    {
        return false;
    }
    outPacket.status = static_cast<ServerAuthResponseStatus>(rawStatus);

    return reader.empty();
}

bool deserializeServerSnapshotFullPacketUnreliable(
    const void* data,
    size_t size,
    ServerSnapshotFullPacketUnreliable& outPacket)
{
    ByteReader reader(data, size);

    uint8_t rawType = 0;
    if (!reader.readU8(rawType))
    {
        return false;
    }

    outPacket.header.type = static_cast<ServerUnreliablePacketType>(rawType);

    if (outPacket.header.type != ServerUnreliablePacketType::SERVER_SNAPSHOT_FULL_PACKET_UNRELIABLE)
    {
        return false;
    }

    if (!reader.readU32(outPacket.snapshotId))
    {
        return false;
    }

    if (!reader.readU64(outPacket.serverTick))
    {
        return false;
    }

    if (!reader.readU64(outPacket.serverTimeNs))
    {
        return false;
    }

    if (!reader.readU32(outPacket.lastProcessedInputSequenceNumber))
    {
        return false;
    }

    if (!reader.empty())
    {
        return false;
    }

    return true;
}
