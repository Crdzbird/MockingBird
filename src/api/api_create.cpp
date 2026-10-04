// auris_create and auris_destroy. The caller's model-path strings are
// borrowed for the duration of the call only, so the engine keeps copies.
#include "auris/auris.h"
#include "auris_log.hpp"
#include "config_validation.hpp"
#include "engine.hpp"
#include "worker/worker_driver.hpp"

#include <memory>
#include <new>

namespace {

void own_model_paths(auris_engine* e, const auris_config_t* cfg) {
    e->cfg.neural_vad_model_path  = nullptr;
    e->cfg.sound_class_model_path = nullptr;
    if (cfg->neural_vad_model_path != nullptr && cfg->neural_vad_model_path[0] != '\0') {
        e->neural_vad_model_path_storage = cfg->neural_vad_model_path;
        e->cfg.neural_vad_model_path     = e->neural_vad_model_path_storage.c_str();
    }
    if (cfg->sound_class_model_path != nullptr && cfg->sound_class_model_path[0] != '\0') {
        e->sound_class_model_path_storage = cfg->sound_class_model_path;
        e->cfg.sound_class_model_path     = e->sound_class_model_path_storage.c_str();
    }
}

void make_tap(auris_engine* e, const auris_config_t* cfg) {
    if (!cfg->enable_pcm_tap) return;
    const size_t capacity = cfg->pcm_tap_frames != 0
        ? static_cast<size_t>(cfg->pcm_tap_frames)
        : static_cast<size_t>(cfg->sample_rate_hz);
    e->pcm_tap = std::make_unique<auris::SpscRing<int16_t>>(capacity);
}

void join_worker(auris_engine* e) {
    if (e->capture_backend) e->capture_backend->stop();
    auris::driver_stop(e);
    e->running.store(false, std::memory_order_release);
}

} // namespace


extern "C" {

AURIS_API auris_status_t auris_create(const auris_config_t* cfg, auris_engine_t** out_engine) {
    if (out_engine != nullptr) *out_engine = nullptr;
    if (cfg == nullptr || out_engine == nullptr) return AURIS_ERR_INVALID_ARG;
    const auris_status_t v = auris::validate_config(*cfg);
    if (v != AURIS_OK) {
        AURIS_LOG("validate_config failed: %d (backend=%d codec=%d vad_tier=%d sr=%u)",
                  static_cast<int>(v), auris::enum_as_int(cfg->backend),
                  auris::enum_as_int(cfg->codec), auris::enum_as_int(cfg->vad_tier),
                  cfg->sample_rate_hz);
        return v;
    }
    try {
        auto encoder = auris::make_encoder(cfg->codec);
        if (encoder == nullptr) return AURIS_ERR_UNSUPPORTED;
        const auris_status_t enc_init = encoder->init(*cfg);
        if (enc_init != AURIS_OK) return enc_init;

        auto e = std::make_unique<auris_engine>(*cfg, cfg->pcm_ring_frames,
                                                cfg->packet_queue_capacity,
                                                encoder->max_packet_bytes());
        e->encoder = std::move(encoder);
        e->wake_batch_samples.store(auris::batch_samples_for(e->cfg, cfg->worker_batch_frames),
                                    std::memory_order_relaxed);
        make_tap(e.get(), cfg);
        own_model_paths(e.get(), cfg);
        if (cfg->backend != AURIS_BACKEND_HOST_PCM) {
            e->capture_backend = auris::make_backend(cfg->backend);
            if (e->capture_backend == nullptr) return AURIS_ERR_UNSUPPORTED;
        }
        *out_engine = e.release();
        return AURIS_OK;
    } catch (const std::bad_alloc&) {
        return AURIS_ERR_NO_MEMORY;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API void auris_destroy(auris_engine_t* engine) {
    if (engine == nullptr) return;
    try {
        std::lock_guard lk(engine->lifecycle_mu);
        if (engine->running.load(std::memory_order_acquire)) join_worker(engine);
    } catch (...) {
    }
    delete engine;
}

} // extern "C"
