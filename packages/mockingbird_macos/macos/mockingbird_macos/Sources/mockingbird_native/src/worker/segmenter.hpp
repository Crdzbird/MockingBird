// Turns per-frame detections into segments: opens on a rising edge, replays
// the pre-roll, emits packets while active, and closes after the hangover.
#pragma once

#include "preroll_ring.hpp"
#include "worker/detection_pair.hpp"
#include "worker/packet_emitter.hpp"

#include <cstdint>

struct mockingbird_engine;

namespace mockingbird {

class Segmenter {
public:
    void init(mockingbird_engine* e, uint32_t frame_samples);

    void on_frame(const int16_t* samples, uint64_t now_ts, const DetectionPair& d,
                  uint32_t hangover_frames);

    void finish(uint64_t now_ts);

    uint64_t open_segment_id() const { return open_id_; }

private:
    void open(uint64_t at_us, float confidence);
    void close(uint64_t at_us);
    void flush_preroll();
    void emit(const int16_t* samples, uint64_t now_ts, uint32_t flags);
    void on_active(const int16_t* samples, uint64_t now_ts, const DetectionPair& d);
    void on_trailing_silence(const int16_t* samples, uint64_t now_ts, uint32_t hangover_frames);
    void emit_speech_end(uint64_t now_ts);

    mockingbird_engine* e_ = nullptr;
    PacketEmitter emitter_;
    PrerollRing   preroll_;

    uint64_t open_id_        = 0;
    uint64_t start_us_       = 0;
    uint32_t pkt_index_      = 0;
    bool     segment_open_   = false;
    bool     had_speech_     = false;
    bool     vad_was_active_ = false;
    bool     ano_was_active_ = false;
    uint32_t silence_frames_ = 0;
};

} // namespace mockingbird
