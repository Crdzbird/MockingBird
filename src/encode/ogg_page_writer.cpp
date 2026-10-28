// Page layout: capture pattern, version, header type, granule position,
// serial number, page sequence, CRC, segment count, lacing values, data.
#include "encode/ogg_page_writer.hpp"

#include "encode/ogg_crc.hpp"

#include <cstring>

namespace mockingbird {
namespace {
void put_le(uint8_t* p, uint64_t v, int bytes) noexcept {
    for (int i = 0; i < bytes; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

}

void OggPageWriter::reset(uint32_t serial) noexcept {
    serial_ = serial;
    sequence_ = 0;
}

int OggPageWriter::write(const uint8_t* packet, size_t size, uint8_t flags, int64_t granule,
                         uint8_t* out, size_t cap) noexcept {
    const size_t lacing = size / 255 + 1;
    if (lacing > 255) return -1;
    if (27 + lacing + size > cap) return -1;
    for (size_t i = 0; i + 1 < lacing; ++i) out[27 + i] = 255;
    out[27 + lacing - 1] = static_cast<uint8_t>(size % 255);
    if (size > 0) std::memcpy(out + 27 + lacing, packet, size);
    return finish(out, lacing, size, flags, granule);
}

int OggPageWriter::write_end(int64_t granule, uint8_t* out, size_t cap) noexcept {
    if (cap < 27) return -1;
    return finish(out, 0, 0, kEndOfStream, granule);
}

int OggPageWriter::finish(uint8_t* out, size_t lacing, size_t size, uint8_t flags,
                          int64_t granule) noexcept {
    const size_t header = 27 + lacing;
    std::memcpy(out, "OggS", 4);
    out[4] = 0;
    out[5] = flags;
    put_le(out + 6, static_cast<uint64_t>(granule), 8);
    put_le(out + 14, serial_, 4);
    put_le(out + 18, sequence_++, 4);
    put_le(out + 22, 0, 4);
    out[26] = static_cast<uint8_t>(lacing);
    put_le(out + 22, OggCrc::compute(out, header + size), 4);
    return static_cast<int>(header + size);
}

}
