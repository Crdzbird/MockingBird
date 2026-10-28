// Detector threshold ranges, checked only for detectors that are enabled,
// and the features this build was compiled with.
#include "config_validation.hpp"

#include <cmath>

namespace mockingbird {
namespace {
bool unit_interval(float v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; }

bool non_negative(float v) { return std::isfinite(v) && v >= 0.0f; }

}

mockingbird_status_t check_detector_ranges(const mockingbird_config_t& c, int vad_tier, int codec) {
    if (c.enable_vad && vad_tier == MOCKINGBIRD_VAD_NEURAL && !unit_interval(c.vad_neural_threshold)) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    if (c.enable_anomaly && (!non_negative(c.anomaly_rms_z) || !non_negative(c.anomaly_flux_z))) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    if (c.enable_sound_class && !unit_interval(c.sound_class_threshold)) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    if (vad_tier == MOCKINGBIRD_VAD_FAST && (c.vad_aggressiveness < 0 || c.vad_aggressiveness > 3)) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    if (codec == MOCKINGBIRD_CODEC_OPUS || codec == MOCKINGBIRD_CODEC_OGG_OPUS) {
        if (c.opus_bitrate_bps < 6000u || c.opus_bitrate_bps > 510000u) return MOCKINGBIRD_ERR_INVALID_ARG;
        if (c.opus_complexity < 0 || c.opus_complexity > 10)           return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    return MOCKINGBIRD_OK;
}

mockingbird_status_t check_build_support(const mockingbird_config_t& c, int vad_tier, int codec) {
#ifndef MOCKINGBIRD_HAVE_NEURAL_VAD
    if (vad_tier == MOCKINGBIRD_VAD_NEURAL) return MOCKINGBIRD_ERR_UNSUPPORTED;
#else
    (void)vad_tier;
#endif
#ifndef MOCKINGBIRD_HAVE_RNNOISE
    if (c.enable_rnnoise) return MOCKINGBIRD_ERR_UNSUPPORTED;
#else
    if (c.enable_rnnoise && (c.sample_rate_hz == 0 || (48000u % c.sample_rate_hz) != 0)) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
#endif
#ifndef MOCKINGBIRD_HAVE_OPUS
    if (codec == MOCKINGBIRD_CODEC_OPUS || codec == MOCKINGBIRD_CODEC_OGG_OPUS) {
        return MOCKINGBIRD_ERR_UNSUPPORTED;
    }
#endif
#ifndef MOCKINGBIRD_HAVE_FLAC
    if (codec == MOCKINGBIRD_CODEC_FLAC) return MOCKINGBIRD_ERR_UNSUPPORTED;
#endif
    (void)c;
    (void)codec;
    return MOCKINGBIRD_OK;
}

}
