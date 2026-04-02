#pragma once

#include <cstddef> // size_t
#include <cstdint> // uint8_t, uint32_t, etc.
#include <string>  // std::string

class ByteReader
{
public:
    ByteReader(const void* data, size_t size);

    bool readU8(uint8_t& outValue);
    bool readU16(uint16_t& outValue);
    bool readU32(uint32_t& outValue);
    bool readU64(uint64_t& outValue);

    bool readBytes(void* outData, size_t size);
    bool readString(std::string& outValue, size_t maxLength);

    bool empty() const;
    size_t remaining() const;

private:
    bool canRead(size_t byteCount) const;

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t offset_ = 0;
};