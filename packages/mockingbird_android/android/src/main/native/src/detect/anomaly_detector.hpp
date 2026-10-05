// Anomaly gate: RMS, spectral flux and spectral centroid z-scored against a
// rolling background that freezes while an event is active.
#pragma once

#include "detect/detector.hpp"
#include "detect/rolling_stats.hpp"
#include "detect/spectral_metrics.hpp"

extern "C" {
#include "pffft.h"
}

#include <cstdint>
#include <vector>

namespace mockingbird {

class AnomalyDetector final : public Detector {
public:
    static constexpr int kFftN = 512;

    AnomalyDetector() = default;
    ~AnomalyDetector() override;

    mockingbird_status_t  init(const mockingbird_config_t& cfg) override;
    DetectionResult process(const int16_t* frame, uint32_t frame_count) override;
    void            retune(const mockingbird_config_t& cfg) override;

private:
    mockingbird_status_t  allocate_fft();
    void            build_window();
    void            teardown();
    void            apply_thresholds(const mockingbird_config_t& cfg);
    SpectralMetrics measure(const int16_t* frame, uint32_t n);
    uint32_t        trigger_bits(const SpectralMetrics& m, float z_rms, float z_flux,
                                 float z_centroid) const;
    void            learn_background(const SpectralMetrics& m, bool active);

    PFFFT_Setup*       setup_    = nullptr;
    float*             padded_   = nullptr;
    float*             spectrum_ = nullptr;
    float*             work_     = nullptr;
    std::vector<float> window_;
    std::vector<float> mag_;
    std::vector<float> prev_mag_;
    uint32_t           frame_samples_ = 0;

    RollingStats rms_stats_;
    RollingStats flux_stats_;
    RollingStats centroid_stats_;

    float    rms_threshold_      = 3.0f;
    float    flux_threshold_     = 3.0f;
    float    centroid_threshold_ = 3.0f;
    float    silence_rms_floor_  = 0.002f;
    uint32_t active_hold_frames_ = 1;
    uint32_t active_hold_        = 0;
    uint32_t frozen_frames_      = 0;
    uint32_t max_freeze_frames_  = 0;
};

} // namespace mockingbird
