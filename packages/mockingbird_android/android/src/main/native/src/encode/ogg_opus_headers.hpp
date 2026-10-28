// The two header packets an Ogg Opus stream starts with (RFC 7845): the
// identification header and the comment header.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mockingbird {
class OggOpusHeaders {
public:
    static constexpr char   kVendor[]   = "mockingbird";
    static constexpr size_t kVendorSize = sizeof(kVendor) - 1;
    static constexpr size_t kHeadSize   = 19;
    static constexpr size_t kTagsSize   = 8 + 4 + kVendorSize + 4;

    static void head(uint8_t* out, uint8_t channels, uint16_t pre_skip,
                     uint32_t input_rate) noexcept {
        std::memcpy(out, "OpusHead", 8);
        out[8] = 1;
        out[9] = channels;
        out[10] = static_cast<uint8_t>(pre_skip);
        out[11] = static_cast<uint8_t>(pre_skip >> 8);
        for (int i = 0; i < 4; ++i) out[12 + i] = static_cast<uint8_t>(input_rate >> (8 * i));
        out[16] = 0;
        out[17] = 0;
        out[18] = 0;
    }

    static void tags(uint8_t* out) noexcept {
        std::memcpy(out, "OpusTags", 8);
        for (size_t i = 0; i < 4; ++i) out[8 + i] = static_cast<uint8_t>(kVendorSize >> (8 * i));
        std::memcpy(out + 12, kVendor, kVendorSize);
        std::memset(out + 12 + kVendorSize, 0, 4);
    }
};

}
