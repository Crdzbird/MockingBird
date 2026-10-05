// mockingbird_update_config: structural fields must match the creating config;
// the tunable subset is copied under tune_mu and published by generation.
#include "mockingbird/mockingbird.h"
#include "config_validation.hpp"
#include "engine.hpp"

namespace {

bool same_structure(const mockingbird_config_t& a, const mockingbird_config_t& b) {
    using mockingbird::enum_as_int;
    return enum_as_int(a.backend) == enum_as_int(b.backend) &&
           enum_as_int(a.sample_format) == enum_as_int(b.sample_format) &&
           enum_as_int(a.vad_tier) == enum_as_int(b.vad_tier) &&
           enum_as_int(a.codec) == enum_as_int(b.codec) &&
           a.sample_rate_hz == b.sample_rate_hz && a.channels == b.channels &&
           a.frame_ms == b.frame_ms && a.enable_rnnoise == b.enable_rnnoise &&
           a.pre_roll_ms == b.pre_roll_ms && a.post_roll_ms == b.post_roll_ms &&
           a.segment_max_ms == b.segment_max_ms &&
           a.anomaly_background_ms == b.anomaly_background_ms &&
           a.sound_class_interval_ms == b.sound_class_interval_ms &&
           a.pcm_ring_frames == b.pcm_ring_frames &&
           a.packet_queue_capacity == b.packet_queue_capacity &&
           a.enable_pcm_tap == b.enable_pcm_tap && a.pcm_tap_frames == b.pcm_tap_frames &&
           a.enable_wait_handle == b.enable_wait_handle;
}

bool enables_missing_detector(const mockingbird_config_t& want, const mockingbird_config_t& have) {
    return (want.enable_vad && !have.enable_vad) ||
           (want.enable_anomaly && !have.enable_anomaly) ||
           (want.enable_sound_class && !have.enable_sound_class);
}

void copy_tunables(mockingbird_config_t& live, const mockingbird_config_t& cfg) {
    live.enable_vad                  = cfg.enable_vad;
    live.enable_anomaly              = cfg.enable_anomaly;
    live.enable_sound_class          = cfg.enable_sound_class;
    live.vad_aggressiveness          = cfg.vad_aggressiveness;
    live.vad_neural_threshold        = cfg.vad_neural_threshold;
    live.vad_hangover_ms             = cfg.vad_hangover_ms;
    live.anomaly_rms_z               = cfg.anomaly_rms_z;
    live.anomaly_flux_z              = cfg.anomaly_flux_z;
    live.sound_class_threshold       = cfg.sound_class_threshold;
    live.silence_gate_dbfs           = cfg.silence_gate_dbfs;
    live.enable_adaptive_degradation = cfg.enable_adaptive_degradation;
    live.degrade_busy_ppm            = cfg.degrade_busy_ppm;
    live.worker_batch_frames         = cfg.worker_batch_frames;
}

} // namespace

extern "C" {

MOCKINGBIRD_API mockingbird_status_t mockingbird_update_config(mockingbird_engine_t* e, const mockingbird_config_t* cfg) {
    if (e == nullptr || cfg == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        const mockingbird_status_t v = mockingbird::validate_config(*cfg);
        if (v != MOCKINGBIRD_OK) return v;
        if (!same_structure(*cfg, e->cfg))               return MOCKINGBIRD_ERR_INVALID_ARG;
        if (enables_missing_detector(*cfg, e->cfg))      return MOCKINGBIRD_ERR_INVALID_ARG;
        {
            std::lock_guard lk(e->tune_mu);
            copy_tunables(e->live_cfg, *cfg);
        }
        e->wake_batch_samples.store(mockingbird::batch_samples_for(e->cfg, cfg->worker_batch_frames),
                                    std::memory_order_relaxed);
        e->tune_generation.fetch_add(1, std::memory_order_release);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

} // extern "C"
