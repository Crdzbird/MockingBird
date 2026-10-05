// The per-frame path: measure, z-score, trigger with a de-flicker hold, and
// learn the background only from non-event frames.
#include "detect/anomaly_detector.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mockingbird {

SpectralMetrics AnomalyDetector::measure(const int16_t* frame, uint32_t n) {
    double sumsq = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        const double s = static_cast<double>(frame[i]) / 32768.0;
        sumsq += s * s;
        padded_[i] = (static_cast<float>(frame[i]) / 32768.0f) * window_[i];
    }
    for (uint32_t i = n; i < static_cast<uint32_t>(kFftN); ++i) padded_[i] = 0.0f;
    pffft_transform_ordered(setup_, padded_, spectrum_, work_, PFFFT_FORWARD);

    const size_t bins = mag_.size();
    mag_[0]           = std::fabs(spectrum_[0]);
    mag_[bins - 1]    = std::fabs(spectrum_[1]);
    for (size_t k = 1; k < bins - 1; ++k) {
        const float re = spectrum_[2 * k];
        const float im = spectrum_[2 * k + 1];
        mag_[k]        = std::sqrt(re * re + im * im);
    }
    double flux = 0.0, mag_sum = 0.0, centroid_num = 0.0;
    for (size_t k = 0; k < bins; ++k) {
        const float d = mag_[k] - prev_mag_[k];
        if (d > 0.0f) flux += static_cast<double>(d);
        mag_sum      += static_cast<double>(mag_[k]);
        centroid_num += static_cast<double>(k) * static_cast<double>(mag_[k]);
    }
    std::memcpy(prev_mag_.data(), mag_.data(), bins * sizeof(float));

    SpectralMetrics m;
    m.rms      = static_cast<float>(std::sqrt(sumsq / static_cast<double>(n)));
    m.flux     = static_cast<float>(flux);
    m.centroid = (mag_sum > 1e-9) ? static_cast<float>(centroid_num / mag_sum) : 0.0f;
    return m;
}

uint32_t AnomalyDetector::trigger_bits(const SpectralMetrics& m, float z_rms, float z_flux,
                                       float z_centroid) const {
    uint32_t reason = 0;
    if (z_rms > rms_threshold_) reason |= MOCKINGBIRD_ANOMALY_RMS;
    if (m.rms > silence_rms_floor_) {
        if (z_flux > flux_threshold_)         reason |= MOCKINGBIRD_ANOMALY_SPECTRAL_FLUX;
        if (z_centroid > centroid_threshold_) reason |= MOCKINGBIRD_ANOMALY_CENTROID;
    }
    return reason;
}

void AnomalyDetector::learn_background(const SpectralMetrics& m, bool active) {
    if (!active || frozen_frames_ >= max_freeze_frames_) {
        rms_stats_.push(m.rms);
        flux_stats_.push(m.flux);
        centroid_stats_.push(m.centroid);
        frozen_frames_ = 0;
        return;
    }
    ++frozen_frames_;
}

DetectionResult AnomalyDetector::process(const int16_t* frame, uint32_t frame_count) {
    DetectionResult r;
    if (setup_ == nullptr || frame == nullptr || frame_count != frame_samples_) return r;

    const SpectralMetrics m = measure(frame, frame_count);
    const float z_rms      = rms_stats_.z_score(m.rms);
    const float z_flux     = flux_stats_.z_score(m.flux);
    const float z_centroid = centroid_stats_.z_score(m.centroid);

    const uint32_t reason  = trigger_bits(m, z_rms, z_flux, z_centroid);
    const bool     tripped = reason != 0;
    if (tripped) {
        active_hold_ = active_hold_frames_;
    } else if (active_hold_ > 0) {
        --active_hold_;
    }
    const bool active = tripped || active_hold_ > 0;
    learn_background(m, active);

    if (active) {
        r.active      = true;
        r.reason_mask = reason;
        r.confidence  = std::tanh(std::max(0.0f, std::max({z_rms, z_flux, z_centroid})) / 6.0f);
    }
    return r;
}

} // namespace mockingbird
