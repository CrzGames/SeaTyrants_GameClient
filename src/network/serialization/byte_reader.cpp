#include "network/serialization/byte_reader.h"

#include <cstring> // for std::memcpy

ByteReader::ByteReader(const void* data, size_t size)
    : data_(static_cast<const uint8_t*>(data))
    , size_(size)
    , offset_(0)
{
}

bool ByteReader::readU8(uint8_t& outValue)
{
    if (!canRead(1))
    {
        return false;
    }

    outValue = data_[offset_];
    offset_ += 1;
    return true;
}

bool ByteReader::readU16(uint16_t& outValue)
{
    if (!canRead(2))
    {
        return false;
    }

    outValue =
        (static_cast<uint16_t>(data_[offset_]) << 8) |
        (static_cast<uint16_t>(data_[offset_ + 1]));

    offset_ += 2;
    return true;
}

bool ByteReader::readU32(uint32_t& outValue)
{
    if (!canRead(4))
    {
        return false;
    }

    outValue =
        (static_cast<uint32_t>(data_[offset_]) << 24) |
        (static_cast<uint32_t>(data_[offset_ + 1]) << 16) |
        (static_cast<uint32_t>(data_[offset_ + 2]) << 8) |
        (static_cast<uint32_t>(data_[offset_ + 3]));

    offset_ += 4;
    return true;
}

bool ByteReader::readU64(uint64_t& outValue)
{
    if (!canRead(8))
    {
        return false;
    }

    outValue =
        (static_cast<uint64_t>(data_[offset_]) << 56) |
        (static_cast<uint64_t>(data_[offset_ + 1]) << 48) |
        (static_cast<uint64_t>(data_[offset_ + 2]) << 40) |
        (static_cast<uint64_t>(data_[offset_ + 3]) << 32) |
        (static_cast<uint64_t>(data_[offset_ + 4]) << 24) |
        (static_cast<uint64_t>(data_[offset_ + 5]) << 16) |
        (static_cast<uint64_t>(data_[offset_ + 6]) << 8) |
        (static_cast<uint64_t>(data_[offset_ + 7]));

    offset_ += 8;
    return true;
}

bool ByteReader::readBytes(void* outData, size_t size)
{
    if (!canRead(size))
    {
        return false;
    }

    std::memcpy(outData, data_ + offset_, size);
    offset_ += size;
    return true;
}

bool ByteReader::readString(std::string& outValue, size_t maxLength)
{
    uint16_t length = 0;
    if (!readU16(length))
    {
        return false;
    }

    if (length > maxLength)
    {
        return false;
    }

    if (!canRead(length))
    {
        return false;
    }

    outValue.assign(reinterpret_cast<const char*>(data_ + offset_), length);
    offset_ += length;
    return true;
}

bool ByteReader::empty() const
{
    return offset_ == size_;
}

size_t ByteReader::remaining() const
{
    return size_ - offset_;
}

bool ByteReader::canRead(size_t byteCount) const
{
    return (offset_ + byteCount) <= size_;
}