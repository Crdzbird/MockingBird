// Segment lifecycle: identifiers, begin and end events, the pre-roll replay
// that moves the segment start back to the oldest buffered frame, and the
// end-of-stream flush when the engine stops with a segment still open.
#include "worker/segmenter.hpp"

#include "engine.hpp"
#include "engine_events.hpp"

namespace mockingbird {
namespace {
uint32_t frames_for(uint32_t ms, uint32_t frame_ms) {
    return ms == 0 ? 0u : (ms + frame_ms - 1u) / frame_ms;
}

}

void Segmenter::init(mockingbird_engine* e, uint32_t frame_samples) {
    e_ = e;
    emitter_.init(e, frame_samples);
    preroll_.init(frames_for(e->cfg.pre_roll_ms, e->cfg.frame_ms), frame_samples);
    post_roll_frames_ = frames_for(e->cfg.post_roll_ms, e->cfg.frame_ms);
    frame_us_         = static_cast<uint64_t>(e->cfg.frame_ms) * 1000u;
    max_us_           = static_cast<uint64_t>(e->cfg.segment_max_ms) * 1000u;
}

void Segmenter::open(uint64_t at_us, float confidence) {
    if (open_id_ != 0) return;
    open_id_   = e_->next_segment_id++;
    start_us_  = at_us;
    pkt_index_ = 0;
    e_->segments_opened.fetch_add(1, std::memory_order_relaxed);
    emit_event(e_, make_event(MOCKINGBIRD_EVT_SEGMENT_BEGIN, open_id_, at_us, 0, confidence));
}

void Segmenter::close(uint64_t at_us) {
    if (open_id_ == 0) return;
    emit_event(e_, make_event(MOCKINGBIRD_EVT_SEGMENT_END, open_id_, at_us));
    e_->segments_closed.fetch_add(1, std::memory_order_relaxed);
    open_id_ = 0;
}

void Segmenter::end_segment(uint64_t at_us) {
    if (speech_open_) emit_speech_end(at_us);
    close(at_us);
    segment_open_   = false;
    speech_open_    = false;
    silence_frames_ = 0;
}

bool Segmenter::reaches_cap(uint64_t now_ts) const {
    return max_us_ != 0 && now_ts + frame_us_ >= start_us_ + max_us_;
}

void Segmenter::emit(const int16_t* samples, uint64_t now_ts, uint32_t flags) {
    emitter_.emit(samples, now_ts, flags, open_id_, start_us_, pkt_index_);
}

void Segmenter::flush_preroll() {
    if (preroll_.empty()) return;
    start_us_ = preroll_.oldest_ts_us();
    preroll_.for_each_oldest_first([&](const PrerollRing::Frame& f) {
        emit(f.samples.data(), f.capture_ts_us, MOCKINGBIRD_PKT_FLAG_PREROLL);
    });
    preroll_.clear();
}

void Segmenter::emit_speech_end(uint64_t now_ts) {
    emit_event(e_, make_event(MOCKINGBIRD_EVT_SPEECH_END, open_id_, now_ts, 0, 1.0f));
    speech_open_   = false;
    speech_end_us_ = now_ts;
}

void Segmenter::finish(uint64_t now_ts) {
    if (segment_open_) emitter_.emit_tail(now_ts, open_id_, start_us_, pkt_index_);
    if (segment_open_ && speech_open_) {
        emit_event(e_, make_event(MOCKINGBIRD_EVT_SPEECH_END, open_id_, now_ts));
    }
    close(now_ts);
    segment_open_ = false;
    speech_open_  = false;
}

}
