// Detector threshold ranges, checked only for detectors that are enabled,
// and the features this build was compiled with.
#include "config_validation.hpp"

#include <cmath>

namespace auris {

namespace {

bool unit_interval(float v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; }

bool non_negative(float v) { return std::isfinite(v) && v >= 0.0f; }

} // namespace

auris_status_t check_detector_ranges(const auris_config_t& c, int vad_tier, int codec) {
    if (c.enable_vad && vad_tier == AURIS_VAD_NEURAL && !unit_interval(c.vad_neural_threshold)) {
        return AURIS_ERR_INVALID_ARG;
    }
    if (c.enable_anomaly && (!non_negative(c.anomaly_rms_z) || !non_negative(c.anomaly_flux_z))) {
        return AURIS_ERR_INVALID_ARG;
    }
    if (c.enable_sound_class && !unit_interval(c.sound_class_threshold)) {
        return AURIS_ERR_INVALID_ARG;
    }
    if (vad_tier == AURIS_VAD_FAST && (c.vad_aggressiveness < 0 || c.vad_aggressiveness > 3)) {
        return AURIS_ERR_INVALID_ARG;
    }
    if (codec == AURIS_CODEC_OPUS) {
        if (c.opus_bitrate_bps < 6000u || c.opus_bitrate_bps > 510000u) return AURIS_ERR_INVALID_ARG;
        if (c.opus_complexity < 0 || c.opus_complexity > 10)           return AURIS_ERR_INVALID_ARG;
    }
    return AURIS_OK;
}

auris_status_t check_build_support(const auris_config_t& c, int vad_tier, int codec) {
#ifndef AURIS_HAVE_NEURAL_VAD
    if (vad_tier == AURIS_VAD_NEURAL) return AURIS_ERR_UNSUPPORTED;
#else
    (void)vad_tier;
#endif
#ifndef AURIS_HAVE_RNNOISE
    if (c.enable_rnnoise) return AURIS_ERR_UNSUPPORTED;
#else
    if (c.enable_rnnoise && (c.sample_rate_hz == 0 || (48000u % c.sample_rate_hz) != 0)) {
        return AURIS_ERR_INVALID_ARG;
    }
#endif
#ifndef AURIS_HAVE_OPUS
    if (codec == AURIS_CODEC_OPUS) return AURIS_ERR_UNSUPPORTED;
#endif
#ifndef AURIS_HAVE_FLAC
    if (codec == AURIS_CODEC_FLAC) return AURIS_ERR_UNSUPPORTED;
#endif
    (void)c;
    (void)codec;
    return AURIS_OK;
}

} // namespace auris
