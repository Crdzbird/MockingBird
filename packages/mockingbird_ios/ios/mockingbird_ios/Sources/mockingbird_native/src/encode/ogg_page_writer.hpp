// Writes one Ogg page (RFC 3533) holding exactly one packet. A packet of up
// to 255 * 255 - 1 bytes fits in a single page's segment table. write_end()
// closes a stream with an end-of-stream page that holds no packet.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mockingbird {
class OggPageWriter {
public:
    static constexpr uint8_t kContinued = 0x01;
    static constexpr uint8_t kBeginOfStream = 0x02;
    static constexpr uint8_t kEndOfStream = 0x04;
    static constexpr size_t kMaxOverhead = 27 + 255;

    void reset(uint32_t serial) noexcept;

    int write(const uint8_t* packet, size_t size, uint8_t flags, int64_t granule,
              uint8_t* out, size_t cap) noexcept;

    int write_end(int64_t granule, uint8_t* out, size_t cap) noexcept;

private:
    int finish(uint8_t* out, size_t lacing, size_t size, uint8_t flags,
               int64_t granule) noexcept;

    uint32_t serial_ = 0;
    uint32_t sequence_ = 0;
};

}
