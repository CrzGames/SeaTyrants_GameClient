#include "network/serialization/byte_writer.h"

#include <stdexcept> // std::runtime_error

void ByteWriter::writeU8(uint8_t value)
{
    buffer_.push_back(value);
}

void ByteWriter::writeU16(uint16_t value)
{
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void ByteWriter::writeU32(uint32_t value)
{
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void ByteWriter::writeU64(uint64_t value)
{
    buffer_.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void ByteWriter::writeBytes(const void* data, size_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    buffer_.insert(buffer_.end(), bytes, bytes + size);
}

void ByteWriter::writeString(const std::string& value)
{
    if (value.size() > UINT16_MAX)
    {
        throw std::runtime_error("ByteWriter::writeString: string too long");
    }

    writeU16(static_cast<uint16_t>(value.size()));
    writeBytes(value.data(), value.size());
}

const uint8_t* ByteWriter::data() const
{
    return buffer_.data();
}

size_t ByteWriter::size() const
{
    return buffer_.size();
}

const std::vector<uint8_t>& ByteWriter::buffer() const
{
    return buffer_;
}