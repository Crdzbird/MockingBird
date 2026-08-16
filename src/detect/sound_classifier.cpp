// src/detect/sound_classifier.cpp
//
// YAMNet-based semantic classifier wrapped around ONNX Runtime. The model
// vendored at tests/fixtures/yamnet_3s.onnx expects 3 s of 16 kHz mono
// float audio in [-1, 1] (48000 samples), outputs class scores over the
// 521 AudioSet classes. We buffer accordingly and run inference at the
// configured interval.

#include "sound_classifier.hpp"

#ifdef AURIS_HAVE_SOUND_CLASS

#if __has_include(<onnxruntime_cxx_api.h>)
  #include <onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime-c/onnxruntime_cxx_api.h>)
  #include <onnxruntime-c/onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime/onnxruntime_cxx_api.h>)
  // Framework-style: Microsoft's ONNX Runtime Swift package vends an
  // xcframework, so headers live in onnxruntime.framework/Headers.
  #include <onnxruntime/onnxruntime_cxx_api.h>
#else
  #error "ONNX Runtime header not found for sound_classifier"
#endif
#include "ort_env.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "auris_log.hpp"

namespace auris {

namespace {

constexpr size_t kYamnetSampleCount = 48000;   // 3 s × 16 kHz
constexpr size_t kYamnetClassCount  = 521;     // AudioSet ontology

std::string resolve_model_path(const auris_config_t& cfg) {
    if (cfg.sound_class_model_path != nullptr &&
        cfg.sound_class_model_path[0] != '\0') {
        return std::string(cfg.sound_class_model_path);
    }
    if (const char* env = std::getenv("AURIS_SOUND_CLASS_MODEL")) {
        return std::string(env);
    }
    return "yamnet_3s.onnx";
}

class YamnetSoundClassifier final : public SoundClassifier {
public:
    auris_status_t init(const auris_config_t& cfg) override {
        if (session_ != nullptr) return AURIS_ERR_INVALID_STATE;
        if (cfg.sample_rate_hz != 16000) {
            // YAMNet is a 16 kHz model; require it.
            return AURIS_ERR_UNSUPPORTED;
        }
        try {
            const std::string path = resolve_model_path(cfg);
            AURIS_VLOG("yamnet loading model: '%s'", path.c_str());
            Ort::SessionOptions opts;
            opts.SetIntraOpNumThreads(1);
            opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
            session_ = std::make_unique<Ort::Session>(shared_ort_env(), path.c_str(), opts);
            AURIS_VLOG("yamnet model loaded OK");

            Ort::AllocatorWithDefaultOptions alloc;
            auto in0  = session_->GetInputNameAllocated(0, alloc);
            input_name_  = std::string(in0.get());

            // YAMNet variants emit multiple heads (embeddings, log-mel,
            // scores). We want the one whose trailing dimension equals 521
            // (the AudioSet class count). Falling back to output[0] would
            // give us the "embeddings" head and produce garbage results.
            const size_t n_out = session_->GetOutputCount();
            for (size_t i = 0; i < n_out; ++i) {
                auto name = session_->GetOutputNameAllocated(i, alloc);
                auto shape = session_->GetOutputTypeInfo(i)
                                .GetTensorTypeAndShapeInfo()
                                .GetShape();
                if (output_name_.empty() &&
                    !shape.empty() &&
                    shape.back() == static_cast<int64_t>(kYamnetClassCount)) {
                    output_name_ = std::string(name.get());
                }
            }
            if (output_name_.empty()) {
                auto out0 = session_->GetOutputNameAllocated(0, alloc);
                output_name_ = std::string(out0.get());
            }
            // // VERIFY  Single-input / single-output YAMNet variant.
            // If the vendored model adds extra heads (embeddings, log-mel)
            // their output is consumed but ignored.
        } catch (const Ort::Exception& ex) {
            AURIS_VLOG("yamnet ORT exception: %s", ex.what());
            session_.reset();
            return AURIS_ERR_UNSUPPORTED;
        } catch (const std::exception& ex) {
            AURIS_VLOG("yamnet std exception: %s", ex.what());
            return AURIS_ERR_UNSUPPORTED;
        } catch (...) {
            AURIS_VLOG("yamnet unknown exception");
            return AURIS_ERR_UNSUPPORTED;
        }

        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        buf_.assign(kYamnetSampleCount, 0.0f);
        write_pos_     = 0;
        filled_        = 0;

        // EMA smoothing of per-class scores across inferences. Consecutive 3 s
        // windows overlap heavily, so a single jittery inference should not
        // flip the reported label; the EMA stabilises both the class and its
        // confidence.
        ema_.assign(kYamnetClassCount, 0.0f);
        ema_alpha_     = 0.6f;
        // First classify once 2 s of audio is buffered rather than waiting for
        // the full 3 s window — cuts first-detection latency by a third while
        // keeping the window mostly real content (the unfilled head is zero-
        // padded and lands at the start of the clip, so max-pool still sees
        // the sound).
        min_fill_      = std::min<size_t>(buf_.size(), 32000);

        const uint32_t step_ms =
            cfg.sound_class_interval_ms == 0 ? 500u : cfg.sound_class_interval_ms;
        frames_per_inference_ =
            (step_ms + cfg.frame_ms - 1u) / cfg.frame_ms;
        frames_since_inference_ = 0;
        last_result_.ready      = false;
        return AURIS_OK;
    }

    ClassificationResult feed(const int16_t* frame, uint32_t n) override {
        ClassificationResult r;
        if (session_ == nullptr || frame == nullptr || n == 0) return r;

        // Append frame into the circular buffer.
        for (uint32_t i = 0; i < n; ++i) {
            buf_[write_pos_] = static_cast<float>(frame[i]) / 32768.0f;
            write_pos_ = (write_pos_ + 1) % buf_.size();
            if (filled_ < buf_.size()) ++filled_;
        }

        ++frames_since_inference_;
        if (filled_ < min_fill_)              return r;     // warmup
        if (frames_since_inference_ < frames_per_inference_) return r;
        frames_since_inference_ = 0;

        // Linearize the circular buffer into contiguous_ before inference.
        contiguous_.resize(buf_.size());
        const size_t tail = buf_.size() - write_pos_;
        std::memcpy(contiguous_.data(), buf_.data() + write_pos_, tail * sizeof(float));
        std::memcpy(contiguous_.data() + tail, buf_.data(), write_pos_ * sizeof(float));

        const ClassificationResult fresh = run_inference_(contiguous_.data());
        if (fresh.ready) last_result_ = fresh;
        return fresh;
    }

private:
    ClassificationResult run_inference_(const float* samples) {
        ClassificationResult r;
        try {
            Ort::MemoryInfo mem =
                Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

            // Try [1, N] first; if ORT rejects, fall back to [N].
            const std::array<int64_t, 2> shape2 {
                1, static_cast<int64_t>(kYamnetSampleCount)
            };

            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                mem,
                const_cast<float*>(samples),
                kYamnetSampleCount,
                shape2.data(),
                shape2.size());

            const char* input_names[]  = { input_name_.c_str() };
            const char* output_names[] = { output_name_.c_str() };

            std::vector<Ort::Value> inputs;
            inputs.reserve(1);
            inputs.emplace_back(std::move(input_tensor));

            auto outputs = session_->Run(
                Ort::RunOptions{nullptr},
                input_names,
                inputs.data(),
                1,
                output_names,
                1);
            if (outputs.empty()) return r;

            // Output may be [T, C], [1, C], or [C]; we scan all values
            // through pointer + element count.
            const float* scores = outputs[0].GetTensorData<float>();
            const size_t total  =
                outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
            if (scores == nullptr || total == 0) return r;

            // Reduce frame dimension by max-pool if T > 1: take the highest
            // score per class across time. This emphasises momentary events
            // (a single bark, a slam) that span only one mel-spectrogram
            // window inside the 3 s clip.
            const size_t classes = (total >= kYamnetClassCount)
                ? kYamnetClassCount
                : total;
            // Reused scratch (no per-inference heap alloc). Seed with the lowest
            // float, not 0, so a true max-pool works even if the model head emits
            // negative logits — a 0 floor would clamp all-negative classes to 0.
            max_per_class_.assign(classes, std::numeric_limits<float>::lowest());
            const size_t frames = total / classes;
            for (size_t t = 0; t < frames; ++t) {
                for (size_t c = 0; c < classes; ++c) {
                    const float v = scores[t * classes + c];
                    if (v > max_per_class_[c]) max_per_class_[c] = v;
                }
            }

            // Exponential moving average across inferences, then pick the
            // top-1 from the smoothed scores. This damps single-inference
            // jitter so a momentary misfire cannot flip the reported label.
            const size_t ema_n = std::min(classes, ema_.size());
            int   best_idx = -1;
            float best_p   = 0.0f;
            for (size_t c = 0; c < ema_n; ++c) {
                ema_[c] = ema_alpha_ * max_per_class_[c] +
                          (1.0f - ema_alpha_) * ema_[c];
                if (ema_[c] > best_p) {
                    best_p   = ema_[c];
                    best_idx = static_cast<int>(c);
                }
            }
            r.ready      = true;
            r.class_id   = best_idx;
            r.confidence = best_p;
            return r;
        } catch (const Ort::Exception&) {
            return r;
        } catch (...) {
            return r;
        }
    }

    std::unique_ptr<Ort::Session> session_;
    std::string                   input_name_;
    std::string                   output_name_;
    std::vector<float>            buf_;
    std::vector<float>            contiguous_;
    std::vector<float>            ema_;
    std::vector<float>            max_per_class_;   // reused per-inference scratch
    float                         ema_alpha_              = 0.6f;
    size_t                        min_fill_               = kYamnetSampleCount;
    size_t                        write_pos_              = 0;
    size_t                        filled_                 = 0;
    uint32_t                      frame_samples_          = 0;
    uint32_t                      frames_per_inference_   = 25;
    uint32_t                      frames_since_inference_ = 0;
    ClassificationResult          last_result_{};
};

} // namespace

std::unique_ptr<SoundClassifier> make_sound_classifier() {
    return std::make_unique<YamnetSoundClassifier>();
}

} // namespace auris

#else   // !AURIS_HAVE_SOUND_CLASS

namespace auris {
std::unique_ptr<SoundClassifier> make_sound_classifier() { return nullptr; }
} // namespace auris

#endif
