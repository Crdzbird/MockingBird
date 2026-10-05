// One relaxed atomic load per frame when nothing changed; the mutex is taken
// only on an actual retune, never while a detector is mid-frame.
#include "worker/tunables.hpp"

#include "auris_log.hpp"
#include "detect/detector.hpp"
#include "engine.hpp"

namespace auris {

namespace {

float threshold_or(float v, float fallback) { return (v > 0.0f && v < 1.0f) ? v : fallback; }

float gate_or(float v) { return v == 0.0f ? -60.0f : v; }

} // namespace

void Tunables::load(const auris_engine* e) {
    frame_ms_        = e->cfg.frame_ms;
    seen_generation_ = e->tune_generation.load(std::memory_order_acquire);
    apply(e->cfg);
}

bool Tunables::refresh(auris_engine* e, Detector* vad, Detector* anomaly) {
    const uint64_t gen = e->tune_generation.load(std::memory_order_acquire);
    if (gen == seen_generation_) return false;
    seen_generation_ = gen;
    auris_config_t live;
    {
        std::lock_guard lk(e->tune_mu);
        live = e->live_cfg;
    }
    apply(live);
    if (vad)     vad->retune(live);
    if (anomaly) anomaly->retune(live);
    AURIS_VLOG("retuned to generation %llu", static_cast<unsigned long long>(gen));
    return true;
}

void Tunables::apply(const auris_config_t& c) {
    vad_enabled          = c.enable_vad != 0;
    anomaly_enabled      = c.enable_anomaly != 0;
    sound_class_enabled  = c.enable_sound_class != 0;
    adaptive_on          = c.enable_adaptive_degradation != 0;
    classifier_threshold = threshold_or(c.sound_class_threshold, 0.4f);
    silence_gate_dbfs    = gate_or(c.silence_gate_dbfs);
    hangover_frames      = c.vad_hangover_ms == 0
        ? 0u
        : (c.vad_hangover_ms + frame_ms_ - 1u) / frame_ms_;
    degrade.configure(c.degrade_busy_ppm);
}

} // namespace auris
