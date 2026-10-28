// YAMNet over ONNX Runtime: a 3 s circular window at 16 kHz, classified
// every interval, with per-class max-pooling over time and an EMA across
// inferences so a single jittery result cannot flip the label.
#pragma once

#include "detect/sound_classifier.hpp"

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

#include "detect/ort_headers.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mockingbird {
class YamnetClassifier final : public SoundClassifier {
public:
    static constexpr size_t kSampleCount = 48000;
    static constexpr size_t kClassCount  = 521;

    mockingbird_status_t       init(const mockingbird_config_t& cfg) override;
    ClassificationResult feed(const int16_t* frame, uint32_t n, const FeedHints& hints) override;

private:
    mockingbird_status_t       load_session(const mockingbird_config_t& cfg);
    void                 select_output_head();
    void                 append(const int16_t* frame, uint32_t n);
    bool                 inference_due(const FeedHints& hints);
    bool                 window_all_quiet() const;
    void                 linearize();
    ClassificationResult run_inference(const float* samples);
    ClassificationResult pick_top(const float* scores, size_t total);

    std::unique_ptr<Ort::Session> session_;
    std::string                   input_name_;
    std::string                   output_name_;
    std::vector<float>            buf_;
    std::vector<float>            contiguous_;
    std::vector<float>            ema_;
    std::vector<float>            max_per_class_;
    float                         ema_alpha_              = 0.6f;
    size_t                        min_fill_               = kSampleCount;
    size_t                        write_pos_              = 0;
    size_t                        filled_                 = 0;
    uint32_t                      frame_samples_          = 0;
    uint32_t                      frames_per_inference_   = 50;
    uint32_t                      frames_since_inference_ = 0;
    uint32_t                      frames_per_window_      = 150;
    uint32_t                      quiet_run_frames_       = 0;
};

}

#endif
