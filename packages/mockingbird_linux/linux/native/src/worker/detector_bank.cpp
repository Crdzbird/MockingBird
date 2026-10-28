// A component that fails to initialise is reported as a backend error and
// dropped; the worker keeps running with it disabled.
#include "worker/detector_bank.hpp"

#include "mockingbird_log.hpp"
#include "engine.hpp"
#include "engine_events.hpp"
#include "worker/tunables.hpp"

namespace mockingbird {
template <typename Component>
bool DetectorBank::prepare(mockingbird_engine* e, std::unique_ptr<Component>& c,
                           [[maybe_unused]] const char* tag) {
    if (!c) {
        MOCKINGBIRD_LOG("'%s' factory returned null (feature not compiled in?)", tag);
        return false;
    }
    const mockingbird_status_t s = c->init(e->cfg);
    MOCKINGBIRD_LOG("'%s' init %s (%d)", tag, s == MOCKINGBIRD_OK ? "OK" : "FAILED", static_cast<int>(s));
    if (s == MOCKINGBIRD_OK) return true;
    emit_backend_error(e, s);
    c.reset();
    return false;
}

void DetectorBank::init(mockingbird_engine* e) {
    if (e->cfg.enable_rnnoise) {
        preproc_ = make_rnnoise_preprocessor();
        prepare(e, preproc_, "rnnoise");
    }
    if (e->cfg.enable_vad) {
        vad_ = make_vad(e->cfg.vad_tier);
        prepare(e, vad_, e->cfg.vad_tier == MOCKINGBIRD_VAD_NEURAL ? "neural-vad" : "fast-vad");
    }
    if (e->cfg.enable_anomaly) {
        anomaly_ = make_anomaly();
        prepare(e, anomaly_, "anomaly");
    }
    if (e->cfg.enable_sound_class) {
        classifier_ = make_sound_classifier();
        prepare(e, classifier_, "sound-class");
    }
    if (!e->keywords.keywords.empty()) init_spotter(e);
}

void DetectorBank::denoise(mockingbird_engine* e, int16_t* frame, uint32_t n, bool quiet) {
    if (!preproc_) return;
    if (quiet) {
        e->preproc_frames_skipped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    preproc_->process(frame, n);
}

DetectionPair DetectorBank::detect(const int16_t* frame, uint32_t n, const Tunables& t) {
    DetectionPair d;
    d.vad_on     = vad_ && t.vad_enabled;
    d.anomaly_on = anomaly_ && t.anomaly_enabled;
    if (d.vad_on)     d.vad     = vad_->process(frame, n);
    if (d.anomaly_on) d.anomaly = anomaly_->process(frame, n);
    return d;
}

void DetectorBank::classify(mockingbird_engine* e, const int16_t* frame, uint32_t n, bool quiet,
                            const Tunables& t, uint64_t segment_id, uint64_t now_ts) {
    if (!classifier_ || !t.sound_class_enabled) return;
    FeedHints hints;
    hints.quiet              = quiet;
    hints.cadence_multiplier = t.degrade.cadence_multiplier();
    hints.inhibit            = t.degrade.inhibit();
    const ClassificationResult r = classifier_->feed(frame, n, hints);
    if (r.ran)     e->sound_class_inferences.fetch_add(1, std::memory_order_relaxed);
    if (r.skipped) e->sound_class_inferences_skipped.fetch_add(1, std::memory_order_relaxed);
    if (r.ready && r.confidence >= t.classifier_threshold && r.class_id >= 0) {
        emit_event(e, make_event(MOCKINGBIRD_EVT_SOUND_CLASS, segment_id, now_ts,
                                 static_cast<uint32_t>(r.class_id), r.confidence));
    }
}

}
