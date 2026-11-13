// Everything the audio thread owns: the jitter buffer, the decoder and a
// carry buffer that hands a decoded packet out across callbacks of any size.
// render() is the only entry and never allocates, locks or blocks.
#pragma once

#include "decode/opus_frame_decoder.hpp"
#include "play/jitter_buffer.hpp"

#include <cstdint>
#include <memory>

namespace mockingbird {
class PlayerRenderer {
public:
    mockingbird_status_t init(const mockingbird_player_config_t& cfg, PacketQueue* queue,
                              PlayerShared* shared);
    void render(int16_t* out, uint32_t frames) noexcept;

private:
    void sync() noexcept;
    void restart() noexcept;
    void fill(uint32_t want) noexcept;
    void decode(PacketSlot* slot) noexcept;
    void conceal(uint32_t frames) noexcept;
    void copy_out(int16_t* out, uint32_t frames) noexcept;
    void publish() noexcept;

    JitterBuffer             jitter_;
    OpusFrameDecoder         opus_;
    PacketQueue*             queue_  = nullptr;
    PlayerShared*            shared_ = nullptr;
    std::unique_ptr<int16_t[]> carry_;
    uint32_t                 carry_frames_ = 0;
    uint32_t                 carry_at_     = 0;
    uint32_t                 max_frames_   = 0;
    uint32_t                 rate_         = 0;
    uint16_t                 channels_     = 1;
    uint64_t                 generation_   = 0;
    uint64_t                 clock_        = 0;
    float                    gain_         = -1.0f;
    bool                     opus_history_ = false;
};

}
