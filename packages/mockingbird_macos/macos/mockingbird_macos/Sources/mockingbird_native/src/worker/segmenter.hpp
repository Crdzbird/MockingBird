// Turns per-frame detections into segments: opens on a rising edge, replays
// the pre-roll, emits packets while active, and closes after the longer of
// the hangover and the post-roll, or when the segment reaches its length cap.
// The packet that closes a segment carries MOCKINGBIRD_PKT_FLAG_LAST.
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
    void end_segment(uint64_t at_us);
    void flush_preroll();
    void emit(const int16_t* samples, uint64_t now_ts, uint32_t flags);
    void on_active(const int16_t* samples, uint64_t now_ts, const DetectionPair& d);
    void on_trailing_silence(const int16_t* samples, uint64_t now_ts, uint32_t hangover_frames);
    void emit_speech_end(uint64_t now_ts);
    bool reaches_cap(uint64_t now_ts) const;

    mockingbird_engine* e_ = nullptr;
    PacketEmitter emitter_;
    PrerollRing   preroll_;

    uint64_t open_id_          = 0;
    uint64_t start_us_         = 0;
    uint64_t frame_us_         = 0;
    uint64_t max_us_           = 0;
    uint32_t post_roll_frames_ = 0;
    uint32_t pkt_index_        = 0;
    bool     segment_open_     = false;
    bool     speech_open_      = false;
    bool     vad_was_active_   = false;
    bool     ano_was_active_   = false;
    bool     cut_              = false;
    uint32_t silence_frames_   = 0;
};

}
