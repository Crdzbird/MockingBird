// Segment lifecycle: identifiers, begin and end events, the pre-roll replay
// that moves the segment start back to the oldest buffered frame.
#include "worker/segmenter.hpp"

#include "engine.hpp"
#include "engine_events.hpp"

namespace auris {

void Segmenter::init(auris_engine* e, uint32_t frame_samples) {
    e_ = e;
    emitter_.init(e, frame_samples);
    const uint32_t preroll_frames = e->cfg.pre_roll_ms == 0
        ? 0u
        : (e->cfg.pre_roll_ms + e->cfg.frame_ms - 1u) / e->cfg.frame_ms;
    preroll_.init(preroll_frames, frame_samples);
}

void Segmenter::open(uint64_t at_us, float confidence) {
    if (open_id_ != 0) return;
    open_id_   = e_->next_segment_id++;
    start_us_  = at_us;
    pkt_index_ = 0;
    e_->segments_opened.fetch_add(1, std::memory_order_relaxed);
    emit_event(e_, make_event(AURIS_EVT_SEGMENT_BEGIN, open_id_, at_us, 0, confidence));
}

void Segmenter::close(uint64_t at_us) {
    if (open_id_ == 0) return;
    emit_event(e_, make_event(AURIS_EVT_SEGMENT_END, open_id_, at_us));
    e_->segments_closed.fetch_add(1, std::memory_order_relaxed);
    open_id_ = 0;
}

void Segmenter::emit(const int16_t* samples, uint64_t now_ts, uint32_t flags) {
    emitter_.emit(samples, now_ts, flags, open_id_, start_us_, pkt_index_);
}

void Segmenter::flush_preroll() {
    if (preroll_.empty()) return;
    start_us_ = preroll_.oldest_ts_us();
    preroll_.for_each_oldest_first([&](const PrerollRing::Frame& f) {
        emit(f.samples.data(), f.capture_ts_us, AURIS_PKT_FLAG_PREROLL);
    });
    preroll_.clear();
}

void Segmenter::emit_speech_end(uint64_t now_ts) {
    emit_event(e_, make_event(AURIS_EVT_SPEECH_END, open_id_, now_ts, 0, 1.0f));
}

void Segmenter::finish(uint64_t now_ts) {
    if (segment_open_ && had_speech_) {
        emit_event(e_, make_event(AURIS_EVT_SPEECH_END, open_id_, now_ts));
    }
    close(now_ts);
}

} // namespace auris
