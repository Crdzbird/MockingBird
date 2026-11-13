// One libopus decoder behind the checks the C ABI promises: rate, channels,
// packet length and output room are validated before libopus runs, so a
// hostile packet can make a call fail but never write past the caller's buffer.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>

struct OpusDecoder;

namespace mockingbird {
class OpusFrameDecoder {
public:
    static constexpr uint32_t kMaxPacketBytes = 65536;

    OpusFrameDecoder() = default;
    ~OpusFrameDecoder();
    OpusFrameDecoder(const OpusFrameDecoder&)            = delete;
    OpusFrameDecoder& operator=(const OpusFrameDecoder&) = delete;

    static bool valid_rate(uint32_t hz) noexcept;
    static uint32_t max_frames_for(uint32_t hz) noexcept { return hz / 1000u * 120u; }
    static int frames_in(const uint8_t* data, uint32_t size, uint32_t hz) noexcept;

    mockingbird_status_t init(uint32_t sample_rate_hz, uint16_t channels) noexcept;
    mockingbird_status_t decode(const uint8_t* data, uint32_t size, int16_t* out,
                                uint32_t max_frames, uint32_t& out_frames) noexcept;
    mockingbird_status_t conceal(uint32_t frames, int16_t* out, uint32_t max_frames,
                                 uint32_t& out_frames) noexcept;
    int packet_frames(const uint8_t* data, uint32_t size) const noexcept;
    void reset() noexcept;

    uint32_t sample_rate() const noexcept { return rate_; }
    uint16_t channels() const noexcept { return channels_; }

private:
    ::OpusDecoder* dec_         = nullptr;
    uint32_t       rate_        = 0;
    uint16_t       channels_    = 0;
    uint32_t       last_frames_ = 0;
};

}
