#pragma once

#include <cstddef> // size_t
#include <cstdint> // uint8_t, uint32_t, etc.
#include <string>  // std::string
#include <vector>  // std::vector

class ByteWriter
{
    public:
        void writeU8(uint8_t value);
        void writeU16(uint16_t value);
        void writeU32(uint32_t value);
        void writeU64(uint64_t value);

        void writeBytes(const void* data, size_t size);
        void writeString(const std::string& value);

        const uint8_t* data() const;
        size_t size() const;
        const std::vector<uint8_t>& buffer() const;

    private:
        std::vector<uint8_t> buffer_;
};