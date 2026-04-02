#include "network/serialization/serialize_packets_client.h"

#include "network/serialization/byte_writer.h"

std::vector<uint8_t> serializeClientSecureSessionHelloPacketReliable(const ClientSecureSessionHelloPacketReliable& packet)
{
    ByteWriter writer;

    writer.writeU8(static_cast<uint8_t>(packet.header.type));
    writer.writeU32(packet.networkProtocolVersion);
    writer.writeBytes(packet.clientPublicKey.data(), packet.clientPublicKey.size());
    writer.writeBytes(packet.clientNonce.data(), packet.clientNonce.size());

    return writer.buffer();
}

std::vector<uint8_t> serializeClientAuthPacketReliable(const ClientAuthPacketReliable& packet)
{
    ByteWriter writer;

    writer.writeU8(static_cast<uint8_t>(packet.header.type));
    writer.writeString(packet.authToken);

    return writer.buffer();
}

std::vector<uint8_t> serializeClientReadyForMatchPacketReliable(const ClientReadyForMatchPacketReliable& packet)
{
    ByteWriter writer;

    writer.writeU8(static_cast<uint8_t>(packet.header.type));

    return writer.buffer();
}

std::vector<uint8_t> serializeClientInputPacketUnreliable(const ClientInputPacketUnreliable& packet)
{
    ByteWriter writer;

    writer.writeU8(static_cast<uint8_t>(packet.header.type));
    writer.writeU32(packet.inputSequenceNumber);
    writer.writeU32(packet.lastReceivedSnapshotId);
    writer.writeU8(packet.movementHeldFlags);
    writer.writeU8(packet.movementPressedFlags);
    writer.writeU8(packet.movementReleasedFlags);
    writer.writeU8(packet.actionHeldFlags);
    writer.writeU8(packet.actionPressedFlags);
    writer.writeU8(packet.actionReleasedFlags);

    return writer.buffer();
}

std::vector<uint8_t> serializeClientClockSyncPacketUnreliable(const ClientClockSyncPacketUnreliable& packet)
{
    ByteWriter writer;

    writer.writeU8(static_cast<uint8_t>(packet.header.type));
    writer.writeU64(packet.clientTimeNs);
    writer.writeU32(packet.lastReceivedSnapshotId);

    return writer.buffer();
}
