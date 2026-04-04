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
    if (!this->canRead(1))
    {
        return false;
    }

    outValue = this->data_[this->offset_];
    this->offset_ += 1;
    return true;
}

bool ByteReader::readU16(uint16_t& outValue)
{
    if (!this->canRead(2))
    {
        return false;
    }

    outValue =
        (static_cast<uint16_t>(this->data_[this->offset_]) << 8) |
        (static_cast<uint16_t>(this->data_[this->offset_ + 1]));

    this->offset_ += 2;
    return true;
}

bool ByteReader::readU32(uint32_t& outValue)
{
    if (!this->canRead(4))
    {
        return false;
    }

    outValue =
        (static_cast<uint32_t>(this->data_[this->offset_]) << 24) |
        (static_cast<uint32_t>(this->data_[this->offset_ + 1]) << 16) |
        (static_cast<uint32_t>(this->data_[this->offset_ + 2]) << 8) |
        (static_cast<uint32_t>(this->data_[this->offset_ + 3]));

    this->offset_ += 4;
    return true;
}

bool ByteReader::readU64(uint64_t& outValue)
{
    if (!this->canRead(8))
    {
        return false;
    }

    outValue =
        (static_cast<uint64_t>(this->data_[this->offset_]) << 56) |
        (static_cast<uint64_t>(this->data_[this->offset_ + 1]) << 48) |
        (static_cast<uint64_t>(this->data_[this->offset_ + 2]) << 40) |
        (static_cast<uint64_t>(this->data_[this->offset_ + 3]) << 32) |
        (static_cast<uint64_t>(this->data_[this->offset_ + 4]) << 24) |
        (static_cast<uint64_t>(this->data_[this->offset_ + 5]) << 16) |
        (static_cast<uint64_t>(this->data_[this->offset_ + 6]) << 8) |
        (static_cast<uint64_t>(this->data_[this->offset_ + 7]));

    this->offset_ += 8;
    return true;
}

bool ByteReader::readBytes(void* outData, size_t size)
{
    if (!this->canRead(size))
    {
        return false;
    }

    std::memcpy(outData, this->data_ + this->offset_, size);
    this->offset_ += size;
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

    if (!this->canRead(length))
    {
        return false;
    }

    outValue.assign(reinterpret_cast<const char*>(this->data_ + this->offset_), length);
    this->offset_ += length;
    return true;
}

bool ByteReader::empty() const
{
    return this->offset_ == this->size_;
}

size_t ByteReader::remaining() const
{
    return this->size_ - this->offset_;
}

bool ByteReader::canRead(size_t byteCount) const
{
    return (this->offset_ + byteCount) <= this->size_;
}