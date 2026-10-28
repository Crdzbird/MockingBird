// Setup, teardown and tuning. The FFT size is fixed at 512, which covers a
// 20 ms frame at 16 kHz with zero padding.
#include "detect/anomaly_detector.hpp"

#include <algorithm>
#include <cmath>

namespace mockingbird {
AnomalyDetector::~AnomalyDetector() { teardown(); }

mockingbird_status_t AnomalyDetector::allocate_fft() {
    setup_ = pffft_new_setup(kFftN, PFFFT_REAL);
    if (setup_ == nullptr) return MOCKINGBIRD_ERR_NO_MEMORY;
    padded_   = static_cast<float*>(pffft_aligned_malloc(kFftN * sizeof(float)));
    spectrum_ = static_cast<float*>(pffft_aligned_malloc(kFftN * sizeof(float)));
    work_     = static_cast<float*>(pffft_aligned_malloc(kFftN * sizeof(float)));
    if (padded_ == nullptr || spectrum_ == nullptr || work_ == nullptr) {
        teardown();
        return MOCKINGBIRD_ERR_NO_MEMORY;
    }
    return MOCKINGBIRD_OK;
}

void AnomalyDetector::build_window() {
    window_.resize(frame_samples_);
    for (uint32_t i = 0; i < frame_samples_; ++i) {
        const float p = static_cast<float>(i) / static_cast<float>(frame_samples_ - 1);
        window_[i]    = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979323846f * p));
    }
}

mockingbird_status_t AnomalyDetector::init(const mockingbird_config_t& cfg) {
    if (setup_ != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    if (frame_samples_ == 0 || frame_samples_ > static_cast<uint32_t>(kFftN)) {
        return MOCKINGBIRD_ERR_UNSUPPORTED;
    }
    const mockingbird_status_t fft = allocate_fft();
    if (fft != MOCKINGBIRD_OK) return fft;
    build_window();

    const size_t bins = static_cast<size_t>(kFftN) / 2u + 1u;
    mag_.assign(bins, 0.0f);
    prev_mag_.assign(bins, 0.0f);

    const uint32_t window_frames = std::max<uint32_t>(
        8, cfg.anomaly_background_ms / std::max<uint32_t>(1, cfg.frame_ms));
    rms_stats_.init(window_frames);
    flux_stats_.init(window_frames);
    centroid_stats_.init(window_frames);
    apply_thresholds(cfg);

    active_hold_frames_ = std::max<uint32_t>(1, 150u / std::max<uint32_t>(1, cfg.frame_ms));
    active_hold_        = 0;
    frozen_frames_      = 0;
    max_freeze_frames_  = window_frames;
    silence_rms_floor_  = 0.002f;
    return MOCKINGBIRD_OK;
}

void AnomalyDetector::retune(const mockingbird_config_t& cfg) { apply_thresholds(cfg); }

void AnomalyDetector::apply_thresholds(const mockingbird_config_t& cfg) {
    rms_threshold_      = cfg.anomaly_rms_z;
    flux_threshold_     = cfg.anomaly_flux_z;
    centroid_threshold_ = cfg.anomaly_flux_z;
}

void AnomalyDetector::teardown() {
    if (setup_)    { pffft_destroy_setup(setup_); setup_ = nullptr; }
    if (padded_)   { pffft_aligned_free(padded_); padded_ = nullptr; }
    if (spectrum_) { pffft_aligned_free(spectrum_); spectrum_ = nullptr; }
    if (work_)     { pffft_aligned_free(work_); work_ = nullptr; }
}

std::unique_ptr<Detector> make_anomaly() { return std::make_unique<AnomalyDetector>(); }

}
