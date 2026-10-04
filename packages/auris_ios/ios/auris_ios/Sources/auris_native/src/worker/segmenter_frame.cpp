// The per-frame decision: with no detector running every frame is active,
// so the engine degrades to passthrough with one always-open segment.
#include "worker/segmenter.hpp"

#include "engine.hpp"
#include "engine_events.hpp"

#include <algorithm>

namespace auris {

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
    vad_was_active_ = d.vad.active;
    ano_was_active_ = d.anomaly.active;
}

void Segmenter::on_active(const int16_t* samples, uint64_t now_ts, const DetectionPair& d) {
    silence_frames_ = 0;
    if (!segment_open_) {
        segment_open_ = true;
        had_speech_   = false;
        open(now_ts, std::max(d.vad.confidence, d.anomaly.confidence));
        flush_preroll();
    }
    if (d.vad_on && d.vad.active && !vad_was_active_) {
        had_speech_ = true;
        emit_event(e_, make_event(AURIS_EVT_SPEECH_BEGIN, open_id_, now_ts, 0,
                                  d.vad.confidence));
    }
    if (d.anomaly_on && d.anomaly.active && !ano_was_active_) {
        emit_event(e_, make_event(AURIS_EVT_ANOMALY, open_id_, now_ts,
                                  d.anomaly.reason_mask, d.anomaly.confidence));
    }
    emit(samples, now_ts, 0u);
}

void Segmenter::on_trailing_silence(const int16_t* samples, uint64_t now_ts,
                                    uint32_t hangover_frames) {
    emit(samples, now_ts, AURIS_PKT_FLAG_POSTROLL);
    ++silence_frames_;
    if (silence_frames_ < hangover_frames) return;
    if (had_speech_) emit_speech_end(now_ts);
    close(now_ts);
    segment_open_   = false;
    had_speech_     = false;
    silence_frames_ = 0;
}

} // namespace auris
