// The model loads on the first verdict if the host has not loaded it
// already through mockingbird_speaker_embedding; a model that fails to load
// is reported once and verification stays off for the rest of the run.
#include "worker/speaker_verifier.hpp"

#include "engine.hpp"
#include "engine_events.hpp"

#include <algorithm>

namespace mockingbird {
void SpeakerVerifier::on_frame(mockingbird_engine* e, const int16_t* frame, uint32_t n, bool speech_open,
                               uint64_t segment_id, uint64_t now_ts) {
    if (failed_ || !e->speaker.active.load(std::memory_order_acquire)) {
        collecting_ = false;
        return;
    }
    if (speech_open && !collecting_) {
        collecting_ = true;
        decided_    = false;
        segment_id_ = segment_id;
        audio_.clear();
    }
    if (speech_open && !decided_) {
        const size_t take = std::min<size_t>(n, kMaxSamples - audio_.size());
        audio_.insert(audio_.end(), frame, frame + take);
        if (audio_.size() >= kMaxSamples) verdict(e, now_ts);
        return;
    }
    if (speech_open || !collecting_) return;
    collecting_ = false;
    if (!decided_) verdict(e, now_ts);
}

void SpeakerVerifier::verdict(mockingbird_engine* e, uint64_t now_ts) {
    decided_ = true;
    if (audio_.size() < SpeakerModel::kMinSamples) return;
    mockingbird_status_t status = MOCKINGBIRD_OK;
    const std::shared_ptr<SpeakerModel> model = e->speaker.loaded(status);
    if (!model) {
        failed_ = true;
        emit_backend_error(e, status);
        return;
    }
    std::vector<float> target;
    float              threshold = 0.5f;
    {
        std::lock_guard lk(e->speaker.mu);
        target    = e->speaker.target;
        threshold = e->speaker.threshold;
    }
    try {
        const float sim = cosine(model->embed(audio_.data(), audio_.size()), target);
        emit_event(e, make_event(MOCKINGBIRD_EVT_SPEAKER, segment_id_, now_ts, sim >= threshold ? 1u : 0u,
                                 std::clamp(sim, 0.0f, 1.0f)));
    } catch (...) {
        failed_ = true;
        emit_backend_error(e, MOCKINGBIRD_ERR_INTERNAL);
    }
}

}
