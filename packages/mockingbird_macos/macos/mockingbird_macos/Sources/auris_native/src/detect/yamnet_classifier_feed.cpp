// Buffering and gating: audio is always buffered so a gated or suspended
// classifier resumes with a full, current window.
#include "detect/yamnet_classifier.hpp"

#ifdef AURIS_HAVE_SOUND_CLASS

#include <cstring>

namespace auris {

void YamnetClassifier::append(const int16_t* frame, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        buf_[write_pos_] = static_cast<float>(frame[i]) / 32768.0f;
        write_pos_       = (write_pos_ + 1) % buf_.size();
        if (filled_ < buf_.size()) ++filled_;
    }
}

bool YamnetClassifier::inference_due(const FeedHints& hints) {
    ++frames_since_inference_;
    if (filled_ < min_fill_) return false;
    const uint32_t multiplier = hints.cadence_multiplier == 0 ? 1u : hints.cadence_multiplier;
    if (frames_since_inference_ < frames_per_inference_ * multiplier) return false;
    frames_since_inference_ = 0;
    return true;
}

bool YamnetClassifier::window_all_quiet() const {
    return quiet_run_frames_ >= frames_per_window_ ||
           static_cast<size_t>(quiet_run_frames_) * frame_samples_ >= filled_;
}

void YamnetClassifier::linearize() {
    contiguous_.resize(buf_.size());
    const size_t tail = buf_.size() - write_pos_;
    std::memcpy(contiguous_.data(), buf_.data() + write_pos_, tail * sizeof(float));
    std::memcpy(contiguous_.data() + tail, buf_.data(), write_pos_ * sizeof(float));
}

ClassificationResult YamnetClassifier::feed(const int16_t* frame, uint32_t n,
                                            const FeedHints& hints) {
    ClassificationResult r;
    if (session_ == nullptr || frame == nullptr || n == 0) return r;
    quiet_run_frames_ = hints.quiet ? quiet_run_frames_ + 1 : 0;
    append(frame, n);
    if (!inference_due(hints)) return r;
    if (hints.inhibit || window_all_quiet()) {
        r.skipped = true;
        return r;
    }
    linearize();
    ClassificationResult fresh = run_inference(contiguous_.data());
    fresh.ran = true;
    return fresh;
}

} // namespace auris

#endif
