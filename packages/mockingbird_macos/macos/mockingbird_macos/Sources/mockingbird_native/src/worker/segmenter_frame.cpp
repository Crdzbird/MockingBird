// The per-frame decision: with no detector running every frame is active,
// so the engine degrades to passthrough with one open segment that the
// length cap splits. A segment cut by the cap starts the next one with fresh
// begin events.
#include "worker/segmenter.hpp"

#include "engine.hpp"
#include "engine_events.hpp"

#include <algorithm>

namespace mockingbird {
namespace {
constexpr uint32_t kLast     = MOCKINGBIRD_PKT_FLAG_LAST;
constexpr uint32_t kPostroll = MOCKINGBIRD_PKT_FLAG_POSTROLL;
}

void Segmenter::on_frame(const int16_t* samples, uint64_t now_ts, const DetectionPair& d,
                         uint32_t hangover_frames) {
    const bool is_active = d.vad.active || d.anomaly.active || !d.running();
    if (is_active) {
        on_active(samples, now_ts, d);
    } else if (segment_open_) {
        on_trailing_silence(samples, now_ts, hangover_frames);
    } else {
        preroll_.push(samples, now_ts);
    }
    vad_was_active_ = d.vad.active && !cut_;
    ano_was_active_ = d.anomaly.active && !cut_;
    cut_            = false;
}

void Segmenter::on_active(const int16_t* samples, uint64_t now_ts, const DetectionPair& d) {
    silence_frames_ = 0;
    if (!segment_open_) {
        segment_open_ = true;
        speech_open_  = false;
        open(now_ts, std::max(d.vad.confidence, d.anomaly.confidence));
        flush_preroll();
    }
    if (d.vad_on && d.vad.active && !vad_was_active_ && !speech_open_) {
        speech_open_ = true;
        const uint64_t floor = std::min(now_ts, std::max(start_us_, speech_end_us_));
        const uint64_t lead  = std::min(d.vad.onset_lead_us, now_ts - floor);
        emit_event(e_, make_event(MOCKINGBIRD_EVT_SPEECH_BEGIN, open_id_, now_ts - lead, 0,
                                  d.vad.confidence));
    }
    if (d.anomaly_on && d.anomaly.active && !ano_was_active_) {
        emit_event(e_, make_event(MOCKINGBIRD_EVT_ANOMALY, open_id_, now_ts,
                                  d.anomaly.reason_mask, d.anomaly.confidence));
    }
    const bool capped = reaches_cap(now_ts);
    emit(samples, now_ts, capped ? kLast : 0u);
    if (!capped) return;
    end_segment(now_ts);
    cut_ = true;
}

void Segmenter::on_trailing_silence(const int16_t* samples, uint64_t now_ts,
                                    uint32_t hangover_frames) {
    ++silence_frames_;
    const bool done = silence_frames_ >= std::max(hangover_frames, post_roll_frames_) ||
                      reaches_cap(now_ts);
    emit(samples, now_ts, kPostroll | (done ? kLast : 0u));
    if (speech_open_ && silence_frames_ >= hangover_frames) emit_speech_end(now_ts);
    if (done) end_segment(now_ts);
}

}
