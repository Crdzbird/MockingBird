// Keyword spotting in the detector bank: built only when keywords were
// added, and reported like any other component that fails to initialise.
// The spotter listens while the input is above the silence gate and, when
// voice detection runs, for 1.5 s after the last voiced frame.
#include "worker/detector_bank.hpp"

#include "mockingbird_log.hpp"
#include "engine.hpp"
#include "engine_events.hpp"

namespace mockingbird {
void DetectorBank::init_spotter(mockingbird_engine* e) {
    spotter_       = make_keyword_spotter();
    listen_frames_ = 1500u / e->cfg.frame_ms;
    const mockingbird_status_t s =
        spotter_ ? spotter_->init(e->cfg, e->keywords) : MOCKINGBIRD_ERR_UNSUPPORTED;
    MOCKINGBIRD_LOG("'keywords' init %s (%d)", s == MOCKINGBIRD_OK ? "OK" : "FAILED", static_cast<int>(s));
    if (s == MOCKINGBIRD_OK) return;
    emit_backend_error(e, s);
    spotter_.reset();
}

void DetectorBank::spot(mockingbird_engine* e, const int16_t* frame, uint32_t n, bool quiet,
                        const DetectionPair& d, uint64_t segment_id, uint64_t now_ts) {
    if (!spotter_) return;
    voice_idle_frames_ = d.vad.active ? 0u : voice_idle_frames_ + (voice_idle_frames_ < UINT32_MAX);
    const bool listen  = !quiet && (!d.vad_on || voice_idle_frames_ < listen_frames_);
    for (const KeywordHit& h : spotter_->feed(frame, n, listen)) {
        emit_event(e, make_event(MOCKINGBIRD_EVT_KEYWORD, segment_id, now_ts, h.id, h.score));
    }
}

}
