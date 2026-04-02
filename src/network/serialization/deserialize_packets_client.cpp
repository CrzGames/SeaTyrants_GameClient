#include "network/serialization/deserialize_packets_client.h"

#include "network/serialization/byte_reader.h"

bool deserializeClientInputPacketUnreliable(const void* data, size_t size, ClientInputPacketUnreliable& outPacket)
{
    ByteReader reader(data, size);

    uint8_t rawType = 0;
    if (!reader.readU8(rawType))
    {
        return false;
    }

    outPacket.header.type = static_cast<ClientUnreliablePacketType>(rawType);

    if (outPacket.header.type != ClientUnreliablePacketType::CLIENT_INPUT_PACKET_UNRELIABLE)
    {
        return false;
    }

    if (!reader.readU32(outPacket.inputSequenceNumber))
    {
        return false;
    }

    if (!reader.readU32(outPacket.lastReceivedSnapshotId))
    {
        return false;
    }

    if (!reader.readU8(outPacket.movementHeldFlags))
    {
        return false;
    }

    if (!reader.readU8(outPacket.movementPressedFlags))
    {
        return false;
    }

    if (!reader.readU8(outPacket.movementReleasedFlags))
    {
        return false;
    }

    if (!reader.readU8(outPacket.actionHeldFlags))
    {
        return false;
    }

    if (!reader.readU8(outPacket.actionPressedFlags))
    {
        return false;
    }

    if (!reader.readU8(outPacket.actionReleasedFlags))
    {
        return false;
    }

    if (!reader.empty())
    {
        return false;
    }

    return true;
}
