// Silero VAD (v5 and v6) through ONNX Runtime. Frames are buffered into the
// model's window (512 samples at 16 kHz, 256 at 8 kHz), fed with the previous
// window's tail as context the way the upstream wrapper does; every call
// returns the cached decision, with hysteresis and onset confirmation.
#pragma once

#include "detect/detector.hpp"

#ifdef MOCKINGBIRD_HAVE_NEURAL_VAD

#include "detect/ort_headers.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace mockingbird {

class NeuralVadDetector final : public Detector {
public:
    NeuralVadDetector() = default;

    mockingbird_status_t  init(const mockingbird_config_t& cfg) override;
    DetectionResult process(const int16_t* frame, uint32_t n) override;
    void            retune(const mockingbird_config_t& cfg) override;

private:
    mockingbird_status_t load_session(const mockingbird_config_t& cfg);
    void           apply_thresholds(const mockingbird_config_t& cfg);
    bool           run_inference(const float* window);
    bool           decide(float p_speech);

    std::unique_ptr<Ort::Session> session_;
    std::vector<float>            buf_;
    std::vector<float>            state_;
    std::vector<float>            context_;
    std::vector<float>            input_;
    uint32_t                      sample_rate_   = 0;
    uint32_t                      frame_samples_ = 0;
    size_t                        window_        = 512;
    float                         threshold_     = 0.5f;
    float                         neg_threshold_ = 0.35f;
    float                         last_p_        = 0.0f;
    int                           onset_count_   = 0;
    int                           onset_needed_  = 2;
    bool                          cached_active_ = false;
};

} // namespace mockingbird

#endif
