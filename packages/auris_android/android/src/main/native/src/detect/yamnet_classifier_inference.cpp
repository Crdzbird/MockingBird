// One YAMNet inference. Scores may arrive as [T, C], [1, C] or [C]; time is
// max-pooled per class so momentary events register, then smoothed.
#include "detect/yamnet_classifier.hpp"

#ifdef AURIS_HAVE_SOUND_CLASS

#include <algorithm>
#include <array>
#include <limits>

namespace auris {

ClassificationResult YamnetClassifier::pick_top(const float* scores, size_t total) {
    const size_t classes = total >= kClassCount ? kClassCount : total;
    max_per_class_.assign(classes, std::numeric_limits<float>::lowest());
    const size_t frames = total / classes;
    for (size_t t = 0; t < frames; ++t) {
        for (size_t c = 0; c < classes; ++c) {
            const float v = scores[t * classes + c];
            if (v > max_per_class_[c]) max_per_class_[c] = v;
        }
    }
    ClassificationResult r;
    const size_t ema_n = std::min(classes, ema_.size());
    int   best_idx = -1;
    float best_p   = 0.0f;
    for (size_t c = 0; c < ema_n; ++c) {
        ema_[c] = ema_alpha_ * max_per_class_[c] + (1.0f - ema_alpha_) * ema_[c];
        if (ema_[c] > best_p) {
            best_p   = ema_[c];
            best_idx = static_cast<int>(c);
        }
    }
    r.ready      = true;
    r.class_id   = best_idx;
    r.confidence = best_p;
    return r;
}

ClassificationResult YamnetClassifier::run_inference(const float* samples) {
    ClassificationResult r;
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 2> shape{1, static_cast<int64_t>(kSampleCount)};
        Ort::Value input = Ort::Value::CreateTensor<float>(
            mem, const_cast<float*>(samples), kSampleCount, shape.data(), shape.size());
        const char* input_names[]  = {input_name_.c_str()};
        const char* output_names[] = {output_name_.c_str()};
        auto outputs = session_->Run(Ort::RunOptions{nullptr}, input_names, &input, 1,
                                     output_names, 1);
        if (outputs.empty()) return r;
        const float* scores = outputs[0].GetTensorData<float>();
        const size_t total  = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        if (scores == nullptr || total == 0) return r;
        return pick_top(scores, total);
    } catch (...) {
        return r;
    }
}

} // namespace auris

#endif
