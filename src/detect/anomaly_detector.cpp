// src/detect/anomaly_detector.cpp
//
// Adaptive anomaly gate. Per-frame metrics:
//   - RMS                   (time-domain energy)
//   - Spectral flux         (frame-to-frame magnitude delta, half-wave rectified)
//   - Spectral centroid     (mass-center of |X|, in bin units)
//
// Each metric maintains a rolling background mean / variance over the last
// `anomaly_background_ms` worth of frames. A frame triggers when its z-score
// against that background exceeds the configured threshold for any metric.
// The reason_mask bits record which metric(s) tripped.
//
// // ASSUMES  PFFFT vendored at third_party/pffft. Real-FFT, ordered output.
// FFT size hard-pinned at 512 — covers 320 samples (16 kHz × 20 ms) with
// zero-padding and is a PFFFT-legal size (2^9).

#include "detector.hpp"

extern "C" {
#include "pffft.h"
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace auris {

namespace {

constexpr int kFftN = 512;

// Sliding-window mean / variance. push() is O(1); both the running sum and
// the running sum-of-squares are maintained, so z_score() is O(1) too.
//
// // PERF-NOTE  Numerically friendly enough for our needs (10..200-sample
// windows of float metrics). For longer windows or extreme dynamic range
// we'd switch to Welford's online algorithm.
class RollingStats {
public:
    void init(size_t window) {
        window_ = std::max<size_t>(window, 4);
        ring_.assign(window_, 0.0f);
        idx_    = 0;
        filled_ = 0;
        sum_    = 0.0;
        sumsq_  = 0.0;
    }

    void push(float v) {
        const double dv = static_cast<double>(v);
        if (filled_ < window_) {
            ring_[idx_] = v;
            sum_   += dv;
            sumsq_ += dv * dv;
            ++filled_;
        } else {
            const double dold = static_cast<double>(ring_[idx_]);
            ring_[idx_] = v;
            sum_   += dv   - dold;
            sumsq_ += dv*dv - dold*dold;
        }
        idx_ = (idx_ + 1) % window_;
    }

    // z-score of v against the buffered window. Returns 0 during warmup so
    // the engine does not false-trigger before there is enough history.
    float z_score(float v) const {
        if (filled_ < window_ / 2) return 0.0f;
        const double mean = sum_ / static_cast<double>(filled_);
        const double var  = std::max(0.0, sumsq_ / static_cast<double>(filled_) - mean * mean);
        // Noise floor on sigma. Pure silence has variance == 0, so without a
        // floor any non-silent burst that follows quiet silence would never
        // be flagged (z = anything / 0 collapsed to 0). The floor ensures a
        // post-silence burst registers proportional to its magnitude.
        const double sigma = std::sqrt(var) + 1e-6;
        return static_cast<float>((static_cast<double>(v) - mean) / sigma);
    }

private:
    std::vector<float> ring_;
    size_t             window_ = 0;
    size_t             idx_    = 0;
    size_t             filled_ = 0;
    double             sum_    = 0.0;
    double             sumsq_  = 0.0;
};

class AnomalyDetector final : public Detector {
public:
    AnomalyDetector() = default;

    ~AnomalyDetector() override {
        teardown();
    }

    auris_status_t init(const auris_config_t& cfg) override {
        if (setup_ != nullptr) return AURIS_ERR_INVALID_STATE;

        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        if (frame_samples_ == 0 || frame_samples_ > static_cast<uint32_t>(kFftN)) {
            return AURIS_ERR_UNSUPPORTED;
        }

        setup_ = pffft_new_setup(kFftN, PFFFT_REAL);
        if (setup_ == nullptr) return AURIS_ERR_NO_MEMORY;

        // PFFFT requires 16-byte (or 64-byte on AVX) aligned buffers via its
        // own allocator.
        padded_   = static_cast<float*>(pffft_aligned_malloc(kFftN * sizeof(float)));
        spectrum_ = static_cast<float*>(pffft_aligned_malloc(kFftN * sizeof(float)));
        work_     = static_cast<float*>(pffft_aligned_malloc(kFftN * sizeof(float)));
        if (padded_ == nullptr || spectrum_ == nullptr || work_ == nullptr) {
            teardown();
            return AURIS_ERR_NO_MEMORY;
        }

        // Hann window for the first frame_samples_ samples.
        window_.resize(frame_samples_);
        for (uint32_t i = 0; i < frame_samples_; ++i) {
            const float p = static_cast<float>(i) /
                            static_cast<float>(frame_samples_ - 1);
            window_[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979323846f * p));
        }

        // Magnitude bins: N/2 + 1 unique values for real FFT.
        const size_t bins = static_cast<size_t>(kFftN) / 2u + 1u;
        mag_.assign(bins, 0.0f);
        prev_mag_.assign(bins, 0.0f);

        // Rolling background window length in frames.
        const uint32_t window_frames =
            std::max<uint32_t>(8, cfg.anomaly_background_ms / std::max<uint32_t>(1, cfg.frame_ms));
        rms_stats_.init(window_frames);
        flux_stats_.init(window_frames);
        centroid_stats_.init(window_frames);

        // Thresholds.
        rms_threshold_ = cfg.anomaly_rms_z;
        flux_threshold_ = cfg.anomaly_flux_z;
        // Centroid uses the same flux z-threshold as a reasonable default;
        // a separate knob can be added later without an ABI break (reserved
        // slots remain in auris_config_t).
        centroid_threshold_ = cfg.anomaly_flux_z;

        // De-flicker hold (~150 ms): once an event trips, keep the gate
        // "active" for this many frames so one acoustic event surfaces as a
        // single rising edge instead of a burst of ANOMALY events, and brief
        // dropouts inside the event don't re-arm it.
        active_hold_frames_ = std::max<uint32_t>(1, 150u / std::max<uint32_t>(1, cfg.frame_ms));
        active_hold_        = 0;
        // Freeze the rolling background while an event is active so a
        // sustained sound (alarm, sustained bark) cannot raise the baseline to
        // meet itself and self-cancel. Cap the freeze at one background window
        // so a genuinely new ambient level is eventually re-learned.
        frozen_frames_      = 0;
        max_freeze_frames_  = window_frames;
        // Spectral metrics are unreliable below a minimum energy: in a quiet
        // room the spectrum is noise-dominated and the centroid wanders,
        // producing phantom z-spikes. ~ -54 dBFS.
        silence_rms_floor_  = 0.002f;

        return AURIS_OK;
    }

    DetectionResult process(const int16_t* frame, uint32_t frame_count) override {
        DetectionResult r;
        if (setup_ == nullptr || frame == nullptr) return r;
        if (frame_count != frame_samples_)         return r;

        // ---- time-domain RMS over the un-windowed frame --------------------
        double sumsq = 0.0;
        for (uint32_t i = 0; i < frame_count; ++i) {
            const double s = static_cast<double>(frame[i]) / 32768.0;
            sumsq += s * s;
        }
        const float rms = static_cast<float>(
            std::sqrt(sumsq / static_cast<double>(frame_count)));

        // ---- window + zero-pad --------------------------------------------
        for (uint32_t i = 0; i < frame_count; ++i) {
            padded_[i] = (static_cast<float>(frame[i]) / 32768.0f) * window_[i];
        }
        for (uint32_t i = frame_count; i < static_cast<uint32_t>(kFftN); ++i) {
            padded_[i] = 0.0f;
        }

        // ---- real FFT ------------------------------------------------------
        pffft_transform_ordered(setup_, padded_, spectrum_, work_, PFFFT_FORWARD);

        // PFFFT ordered real-FFT output: spectrum_[0] = DC (real),
        // spectrum_[1] = Nyquist (real), then interleaved (re, im) for bins
        // 1..N/2-1.
        const size_t bins = mag_.size();
        mag_[0]            = std::fabs(spectrum_[0]);
        mag_[bins - 1]     = std::fabs(spectrum_[1]);
        for (size_t k = 1; k < bins - 1; ++k) {
            const float re = spectrum_[2 * k];
            const float im = spectrum_[2 * k + 1];
            mag_[k] = std::sqrt(re * re + im * im);
        }

        // ---- spectral metrics ---------------------------------------------
        double flux = 0.0;
        double mag_sum = 0.0;
        double centroid_num = 0.0;
        for (size_t k = 0; k < bins; ++k) {
            const float d = mag_[k] - prev_mag_[k];
            if (d > 0.0f) flux += static_cast<double>(d);
            mag_sum      += static_cast<double>(mag_[k]);
            centroid_num += static_cast<double>(k) * static_cast<double>(mag_[k]);
        }
        const float spectral_flux = static_cast<float>(flux);
        const float spectral_centroid =
            (mag_sum > 1e-9) ? static_cast<float>(centroid_num / mag_sum)
                             : 0.0f;

        std::memcpy(prev_mag_.data(), mag_.data(), bins * sizeof(float));

        // ---- z-score against rolling background ---------------------------
        const float z_rms      = rms_stats_.z_score(rms);
        const float z_flux     = flux_stats_.z_score(spectral_flux);
        const float z_centroid = centroid_stats_.z_score(spectral_centroid);

        // ---- trigger ------------------------------------------------------
        // The energy metric (RMS z) is always live — a sharp rise above the
        // background IS the signal. The spectral metrics only count when the
        // frame carries real energy, suppressing phantom trips in silence.
        const bool has_energy = rms > silence_rms_floor_;
        uint32_t reason = 0;
        if (z_rms > rms_threshold_) reason |= AURIS_ANOMALY_RMS;
        if (has_energy) {
            if (z_flux     > flux_threshold_)     reason |= AURIS_ANOMALY_SPECTRAL_FLUX;
            if (z_centroid > centroid_threshold_) reason |= AURIS_ANOMALY_CENTROID;
        }

        const bool tripped = (reason != 0);
        if (tripped) {
            active_hold_ = active_hold_frames_;
        } else if (active_hold_ > 0) {
            --active_hold_;
        }
        const bool active = tripped || active_hold_ > 0;

        // Update the background ONLY on non-event frames (with a freeze cap),
        // so event audio cannot pollute the baseline it is measured against.
        if (!active || frozen_frames_ >= max_freeze_frames_) {
            rms_stats_.push(rms);
            flux_stats_.push(spectral_flux);
            centroid_stats_.push(spectral_centroid);
            frozen_frames_ = 0;
        } else {
            ++frozen_frames_;
        }

        if (active) {
            r.active      = true;
            // reason_mask is the live trigger bits on a trip frame, and 0 while
            // coasting on the de-flicker hold. The engine only emits ANOMALY on
            // the rising edge (a trip frame), so a host never sees a coasting
            // 0 — locked by test_anomaly_reason.cpp.
            r.reason_mask = reason;
            // Use the maximum z-score across metrics, clamped to [0,1] by a
            // tanh-style squash so the host gets a usable confidence.
            const float max_z = std::max({z_rms, z_flux, z_centroid});
            r.confidence  = std::tanh(std::max(0.0f, max_z) / 6.0f);
        }
        return r;
    }

private:
    void teardown() {
        if (setup_)    { pffft_destroy_setup(setup_); setup_    = nullptr; }
        if (padded_)   { pffft_aligned_free(padded_); padded_   = nullptr; }
        if (spectrum_) { pffft_aligned_free(spectrum_); spectrum_ = nullptr; }
        if (work_)     { pffft_aligned_free(work_); work_     = nullptr; }
    }

    PFFFT_Setup*       setup_         = nullptr;
    float*             padded_        = nullptr;
    float*             spectrum_      = nullptr;
    float*             work_          = nullptr;
    std::vector<float> window_;
    std::vector<float> mag_;
    std::vector<float> prev_mag_;
    uint32_t           frame_samples_ = 0;

    RollingStats       rms_stats_;
    RollingStats       flux_stats_;
    RollingStats       centroid_stats_;

    float              rms_threshold_      = 3.0f;
    float              flux_threshold_     = 3.0f;
    float              centroid_threshold_ = 3.0f;

    float              silence_rms_floor_  = 0.002f;
    uint32_t           active_hold_frames_ = 1;
    uint32_t           active_hold_        = 0;
    uint32_t           frozen_frames_      = 0;
    uint32_t           max_freeze_frames_  = 0;
};

} // namespace

std::unique_ptr<Detector> make_anomaly() {
    return std::make_unique<AnomalyDetector>();
}

} // namespace auris
