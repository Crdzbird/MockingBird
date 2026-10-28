// The CRC-32 Ogg pages carry: polynomial 0x04C11DB7, unreflected, zero
// initial value and no final XOR, computed over the page with its CRC
// field zeroed.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mockingbird {
class OggCrc {
public:
    static uint32_t compute(const uint8_t* data, size_t n) noexcept {
        static const std::array<uint32_t, 256> table = build();
        uint32_t crc = 0;
        for (size_t i = 0; i < n; ++i) {
            crc = (crc << 8) ^ table[((crc >> 24) ^ data[i]) & 0xFFu];
        }
        return crc;
    }

private:
    static std::array<uint32_t, 256> build() noexcept {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t r = i << 24;
            for (int k = 0; k < 8; ++k) {
                r = (r & 0x80000000u) ? (r << 1) ^ 0x04C11DB7u : (r << 1);
            }
            t[i] = r;
        }
        return t;
    }
};

}
