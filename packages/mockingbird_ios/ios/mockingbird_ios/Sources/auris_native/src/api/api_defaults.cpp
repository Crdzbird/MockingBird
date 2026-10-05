// auris_config_defaults: the documented defaults, with every reserved slot
// zeroed so the struct can grow additively.
#include "auris/auris.h"
#include "config_validation.hpp"

extern "C" {

AURIS_API auris_status_t auris_config_defaults(auris_config_t* out_cfg) {
    if (out_cfg == nullptr) return AURIS_ERR_INVALID_ARG;
    *out_cfg = auris_config_t{};
    out_cfg->backend                     = AURIS_BACKEND_AUTO;
    out_cfg->sample_rate_hz              = 16000;
    out_cfg->channels                    = auris::kChannelsV1;
    out_cfg->sample_format               = AURIS_SAMPLE_S16;
    out_cfg->frame_ms                    = 20;
    out_cfg->enable_rnnoise              = 0;
    out_cfg->enable_vad                  = 1;
    out_cfg->vad_tier                    = AURIS_VAD_FAST;
    out_cfg->vad_aggressiveness          = 2;
    out_cfg->vad_neural_threshold        = 0.5f;
    out_cfg->vad_hangover_ms             = 300;
    out_cfg->enable_anomaly              = 1;
    out_cfg->anomaly_rms_z               = 3.0f;
    out_cfg->anomaly_flux_z              = 3.0f;
    out_cfg->anomaly_background_ms       = 2000;
    out_cfg->pre_roll_ms                 = 300;
    out_cfg->post_roll_ms                = 300;
    out_cfg->segment_max_ms              = 60000;
    out_cfg->codec                       = AURIS_CODEC_PCM_S16LE;
    out_cfg->opus_bitrate_bps            = 32000;
    out_cfg->opus_vbr                    = 1;
    out_cfg->opus_complexity             = 5;
    out_cfg->pcm_ring_frames             = 1u << 14;
    out_cfg->packet_queue_capacity       = 256;
    out_cfg->enable_wait_handle          = 0;
    out_cfg->neural_vad_model_path       = nullptr;
    out_cfg->enable_sound_class          = 0;
    out_cfg->sound_class_model_path      = nullptr;
    out_cfg->sound_class_threshold       = 0.4f;
    out_cfg->sound_class_interval_ms     = 1000;
    out_cfg->enable_pcm_tap              = 0;
    out_cfg->pcm_tap_frames              = 0;
    out_cfg->silence_gate_dbfs           = -60.0f;
    out_cfg->enable_adaptive_degradation = 0;
    out_cfg->degrade_busy_ppm            = 700000;
    out_cfg->worker_batch_frames         = 0;
    return AURIS_OK;
}

} // extern "C"
