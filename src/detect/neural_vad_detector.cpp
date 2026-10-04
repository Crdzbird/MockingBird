// Session setup, buffering and tuning; without AURIS_HAVE_NEURAL_VAD the
// factory returns null and the engine reports the tier as unsupported.
#include "detect/neural_vad_detector.hpp"

#ifdef AURIS_HAVE_NEURAL_VAD

#include "auris_log.hpp"
#include "detect/model_path.hpp"
#include "detect/ort_env.hpp"

#include <algorithm>

namespace auris {

auris_status_t NeuralVadDetector::load_session(const auris_config_t& cfg) {
    try {
        const std::string path =
            resolve_model_path(cfg.neural_vad_model_path, "AURIS_NEURAL_VAD_MODEL", "silero_vad.onnx");
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        apply_execution_provider(opts);
        AURIS_VLOG("neural-vad loading model: '%s'", path.c_str());
        session_ = std::make_unique<Ort::Session>(shared_ort_env(), path.c_str(), opts);
        return AURIS_OK;
    } catch (const Ort::Exception& ex) {
        AURIS_VLOG("neural-vad ORT exception: %s", ex.what());
    } catch (const std::exception& ex) {
        AURIS_VLOG("neural-vad exception: %s", ex.what());
    } catch (...) {
    }
    session_.reset();
    return AURIS_ERR_UNSUPPORTED;
}

auris_status_t NeuralVadDetector::init(const auris_config_t& cfg) {
    if (session_ != nullptr) return AURIS_ERR_INVALID_STATE;
    if (cfg.sample_rate_hz != 16000 && cfg.sample_rate_hz != 8000) return AURIS_ERR_UNSUPPORTED;
    const auris_status_t s = load_session(cfg);
    if (s != AURIS_OK) return s;
    sample_rate_   = cfg.sample_rate_hz;
    frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    window_        = cfg.sample_rate_hz == 16000 ? size_t{512} : size_t{256};
    buf_.reserve(window_ + frame_samples_);
    state_.assign(2 * 1 * 128, 0.0f);
    apply_thresholds(cfg);
    onset_needed_  = 2;
    onset_count_   = 0;
    last_p_        = 0.0f;
    cached_active_ = false;
    return AURIS_OK;
}

DetectionResult NeuralVadDetector::process(const int16_t* frame, uint32_t n) {
    DetectionResult r;
    if (session_ == nullptr || frame == nullptr || n == 0) return r;
    const size_t old_size = buf_.size();
    buf_.resize(old_size + n);
    for (uint32_t i = 0; i < n; ++i) buf_[old_size + i] = static_cast<float>(frame[i]) / 32768.0f;
    while (buf_.size() >= window_) {
        cached_active_ = run_inference(buf_.data());
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(window_));
    }
    r.active     = cached_active_;
    r.confidence = std::min(1.0f, std::max(0.0f, last_p_));
    return r;
}

void NeuralVadDetector::retune(const auris_config_t& cfg) { apply_thresholds(cfg); }

void NeuralVadDetector::apply_thresholds(const auris_config_t& cfg) {
    threshold_ = (cfg.vad_neural_threshold > 0.0f && cfg.vad_neural_threshold < 1.0f)
        ? cfg.vad_neural_threshold
        : 0.5f;
    neg_threshold_ = std::max(threshold_ - 0.15f, 0.05f);
}

std::unique_ptr<Detector> make_neural_vad() { return std::make_unique<NeuralVadDetector>(); }

} // namespace auris

#else

namespace auris {
std::unique_ptr<Detector> make_neural_vad() { return nullptr; }
} // namespace auris

#endif
