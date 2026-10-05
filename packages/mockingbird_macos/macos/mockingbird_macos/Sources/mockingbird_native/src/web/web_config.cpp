// Config construction for the WebAssembly shim: defaults, one setter keyed
// by field id, and create() with the status returned by pointer.
#ifdef __EMSCRIPTEN__

#include "mockingbird/mockingbird.h"
#include "web/web_fields.hpp"

#include <emscripten/emscripten.h>

#include <cstdlib>

using namespace mockingbird::web;

namespace {

template <typename T>
void assign(T& field, double v) {
    field = static_cast<T>(v);
}

template <typename E>
void assign_enum(E& field, double v) {
    field = static_cast<E>(static_cast<int>(v));
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE mockingbird_config_t* mockingbird_web_config_new(void) {
    auto* cfg = static_cast<mockingbird_config_t*>(std::calloc(1, sizeof(mockingbird_config_t)));
    if (cfg != nullptr) mockingbird_config_defaults(cfg);
    return cfg;
}

EMSCRIPTEN_KEEPALIVE void mockingbird_web_config_free(mockingbird_config_t* cfg) { std::free(cfg); }

EMSCRIPTEN_KEEPALIVE int mockingbird_web_config_field_count(void) { return kConfigFieldCount; }

EMSCRIPTEN_KEEPALIVE int mockingbird_web_config_set(mockingbird_config_t* c, int field, double v) {
    if (c == nullptr) return -1;
    switch (field) {
        case kBackend:                   assign_enum(c->backend, v); break;
        case kSampleRateHz:              assign(c->sample_rate_hz, v); break;
        case kFrameMs:                   assign(c->frame_ms, v); break;
        case kEnableRnnoise:             assign(c->enable_rnnoise, v); break;
        case kEnableVad:                 assign(c->enable_vad, v); break;
        case kVadTier:                   assign_enum(c->vad_tier, v); break;
        case kVadAggressiveness:         assign(c->vad_aggressiveness, v); break;
        case kVadNeuralThreshold:        assign(c->vad_neural_threshold, v); break;
        case kVadHangoverMs:             assign(c->vad_hangover_ms, v); break;
        case kEnableAnomaly:             assign(c->enable_anomaly, v); break;
        case kAnomalyRmsZ:               assign(c->anomaly_rms_z, v); break;
        case kAnomalyFluxZ:              assign(c->anomaly_flux_z, v); break;
        case kAnomalyBackgroundMs:       assign(c->anomaly_background_ms, v); break;
        case kEnableSoundClass:          assign(c->enable_sound_class, v); break;
        case kSoundClassThreshold:       assign(c->sound_class_threshold, v); break;
        case kSoundClassIntervalMs:      assign(c->sound_class_interval_ms, v); break;
        case kPreRollMs:                 assign(c->pre_roll_ms, v); break;
        case kPostRollMs:                assign(c->post_roll_ms, v); break;
        case kSegmentMaxMs:              assign(c->segment_max_ms, v); break;
        case kCodec:                     assign_enum(c->codec, v); break;
        case kOpusBitrateBps:            assign(c->opus_bitrate_bps, v); break;
        case kOpusVbr:                   assign(c->opus_vbr, v); break;
        case kOpusComplexity:            assign(c->opus_complexity, v); break;
        case kPcmRingFrames:             assign(c->pcm_ring_frames, v); break;
        case kPacketQueueCapacity:       assign(c->packet_queue_capacity, v); break;
        case kEnablePcmTap:              assign(c->enable_pcm_tap, v); break;
        case kPcmTapFrames:              assign(c->pcm_tap_frames, v); break;
        case kSilenceGateDbfs:           assign(c->silence_gate_dbfs, v); break;
        case kEnableAdaptiveDegradation: assign(c->enable_adaptive_degradation, v); break;
        case kDegradeBusyPpm:            assign(c->degrade_busy_ppm, v); break;
        case kWorkerBatchFrames:         assign(c->worker_batch_frames, v); break;
        default:                         return -1;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE mockingbird_engine_t* mockingbird_web_create(const mockingbird_config_t* cfg, int* out_status) {
    mockingbird_engine_t* engine = nullptr;
    const mockingbird_status_t s = mockingbird_create(cfg, &engine);
    if (out_status != nullptr) *out_status = static_cast<int>(s);
    return s == MOCKINGBIRD_OK ? engine : nullptr;
}

} // extern "C"

#endif
