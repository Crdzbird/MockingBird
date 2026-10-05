// Session setup and cadence. The scores head is the output whose last
// dimension is the class count; other heads carry embeddings.
#include "detect/yamnet_classifier.hpp"

#ifdef AURIS_HAVE_SOUND_CLASS

#include "auris_log.hpp"
#include "detect/model_path.hpp"
#include "detect/ort_env.hpp"

#include <algorithm>

namespace auris {

auris_status_t YamnetClassifier::load_session(const auris_config_t& cfg) {
    try {
        const std::string path = resolve_model_path(cfg.sound_class_model_path,
                                                    "AURIS_SOUND_CLASS_MODEL", "yamnet_3s.onnx");
        AURIS_VLOG("yamnet loading model: '%s'", path.c_str());
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        apply_execution_provider(opts);
        session_ = std::make_unique<Ort::Session>(shared_ort_env(), path.c_str(), opts);
        select_output_head();
        return AURIS_OK;
    } catch (const Ort::Exception& ex) {
        AURIS_VLOG("yamnet ORT exception: %s", ex.what());
    } catch (const std::exception& ex) {
        AURIS_VLOG("yamnet exception: %s", ex.what());
    } catch (...) {
    }
    session_.reset();
    return AURIS_ERR_UNSUPPORTED;
}

void YamnetClassifier::select_output_head() {
    Ort::AllocatorWithDefaultOptions alloc;
    input_name_ = std::string(session_->GetInputNameAllocated(0, alloc).get());
    const size_t n_out = session_->GetOutputCount();
    for (size_t i = 0; i < n_out && output_name_.empty(); ++i) {
        auto name  = session_->GetOutputNameAllocated(i, alloc);
        auto shape = session_->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
        if (!shape.empty() && shape.back() == static_cast<int64_t>(kClassCount)) {
            output_name_ = std::string(name.get());
        }
    }
    if (output_name_.empty()) {
        output_name_ = std::string(session_->GetOutputNameAllocated(0, alloc).get());
    }
}

auris_status_t YamnetClassifier::init(const auris_config_t& cfg) {
    if (session_ != nullptr) return AURIS_ERR_INVALID_STATE;
    if (cfg.sample_rate_hz != 16000) return AURIS_ERR_UNSUPPORTED;
    const auris_status_t s = load_session(cfg);
    if (s != AURIS_OK) return s;

    frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    buf_.assign(kSampleCount, 0.0f);
    write_pos_ = 0;
    filled_    = 0;
    ema_.assign(kClassCount, 0.0f);
    ema_alpha_         = 0.6f;
    min_fill_          = std::min<size_t>(buf_.size(), 32000);
    frames_per_window_ = (static_cast<uint32_t>(kSampleCount) + frame_samples_ - 1u) / frame_samples_;
    quiet_run_frames_  = 0;
    const uint32_t step_ms = cfg.sound_class_interval_ms == 0 ? 1000u : cfg.sound_class_interval_ms;
    frames_per_inference_   = (step_ms + cfg.frame_ms - 1u) / cfg.frame_ms;
    frames_since_inference_ = 0;
    return AURIS_OK;
}

std::unique_ptr<SoundClassifier> make_sound_classifier() {
    return std::make_unique<YamnetClassifier>();
}

} // namespace auris

#else

namespace auris {
std::unique_ptr<SoundClassifier> make_sound_classifier() { return nullptr; }
} // namespace auris

#endif
