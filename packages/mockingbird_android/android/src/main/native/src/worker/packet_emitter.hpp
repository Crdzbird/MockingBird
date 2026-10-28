// Encodes one frame into a pool slot and commits it to the host queue.
#pragma once

#include <cstdint>

struct mockingbird_engine;

namespace mockingbird {
class PacketEmitter {
public:
    void init(mockingbird_engine* e, uint32_t frame_samples);

    void emit(const int16_t* samples, uint64_t now_ts, uint32_t flags,
              uint64_t segment_id, uint64_t segment_start_us, uint32_t& segment_pkt_index);

private:
    void overrun(uint64_t now_ts);

    mockingbird_engine* e_             = nullptr;
    uint32_t      frame_samples_ = 0;
};

}
