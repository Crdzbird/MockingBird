// src/detect/neural_vad_detector.cpp
//
// Silero VAD v5 via ONNX Runtime. Compiled only when AURIS_HAVE_NEURAL_VAD
// is defined (i.e. when the build system has linked onnxruntime). When the
// macro is absent, make_neural_vad returns nullptr so the factory surfaces
// AURIS_ERR_UNSUPPORTED at validate_config time.
//
// Model surface (Silero v5):
//   inputs:  "input" [1, 512] f32 (audio in [-1, 1])
//            "sr"     []      i64 (sample rate, 16000 or 8000)
//            "state"  [2, 1, 128] f32 (LSTM state; carry across calls)
//   outputs: "output" [1, 1] f32 (P(speech))
//            "stateN" [2, 1, 128] f32 (next state)
//
// Auris pushes frames of 320 samples (20 ms at 16 kHz); Silero wants 512.
// We buffer until we have a window, run inference once, and slide the
// buffer forward. The detector returns its CURRENT cached decision on
// every call so the worker sees a per-frame answer without latency spikes.
//
// // ASSUMES  Model path:
//   1. AURIS_NEURAL_VAD_MODEL env var if set
//   2. <cwd>/silero_vad.onnx
// init() returns AURIS_ERR_UNSUPPORTED if neither resolves. A future
// step exposes this via the config struct.

#include "detector.hpp"

#ifdef AURIS_HAVE_NEURAL_VAD

// ONNX Runtime header lives in different places depending on integration:
//   - Homebrew on macOS:      <onnxruntime_cxx_api.h>
//   - CocoaPods onnxruntime-c: <onnxruntime-c/onnxruntime_cxx_api.h>
//   - Android Prefab AAR:     <onnxruntime_cxx_api.h>
#if __has_include(<onnxruntime_cxx_api.h>)
  #include <onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime-c/onnxruntime_cxx_api.h>)
  #include <onnxruntime-c/onnxruntime_cxx_api.h>
#else
  #error "ONNX Runtime header not found; check pod / find_package wiring"
#endif
#include <cstdio>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "auris_log.hpp"

namespace auris {

namespace {

constexpr size_t kSileroWindow = 512;   // samples per inference at 16 kHz
constexpr size_t kStateDim     = 2 * 1 * 128;

std::string resolve_model_path(const auris_config_t& cfg) {
    if (cfg.neural_vad_model_path != nullptr &&
        cfg.neural_vad_model_path[0] != '\0') {
        return std::string(cfg.neural_vad_model_path);
    }
    if (const char* env = std::getenv("AURIS_NEURAL_VAD_MODEL")) {
        return std::string(env);
    }
    return "silero_vad.onnx";
}

class NeuralVadDetector final : public Detector {
public:
    NeuralVadDetector() = default;

    auris_status_t init(const auris_config_t& cfg) override {
        if (session_ != nullptr) return AURIS_ERR_INVALID_STATE;
        if (cfg.sample_rate_hz != 16000 && cfg.sample_rate_hz != 8000) {
            return AURIS_ERR_UNSUPPORTED;
        }
        try {
            const std::string path = resolve_model_path(cfg);
            env_     = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING,
                                                  "auris.neural_vad");
            Ort::SessionOptions opts;
            opts.SetIntraOpNumThreads(1);
            opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
            // // VERIFY  Model surface (Silero v5.1.2 pinned):
            //   input[0]  "input"  shape [-1,-1] float32
            //   input[1]  "state"  shape [2,-1,128] float32
            //   input[2]  "sr"     shape [] int64
            //   output[0] "output" shape [-1,1] float32
            //   output[1] "stateN" shape [-1,-1,-1] float32 (runtime [2,1,128])
            AURIS_VLOG("neural-vad loading model: '%s'", path.c_str());
            session_ = std::make_unique<Ort::Session>(*env_, path.c_str(), opts);
            AURIS_VLOG("neural-vad model loaded OK");
        } catch (const Ort::Exception& ex) {
            AURIS_VLOG("neural-vad ORT exception: %s", ex.what());
            session_.reset();
            env_.reset();
            return AURIS_ERR_UNSUPPORTED;
        } catch (const std::exception& ex) {
            AURIS_VLOG("neural-vad std exception: %s", ex.what());
            return AURIS_ERR_UNSUPPORTED;
        } catch (...) {
            AURIS_VLOG("neural-vad unknown exception");
            return AURIS_ERR_UNSUPPORTED;
        }

        sample_rate_   = cfg.sample_rate_hz;
        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        // Silero v5 fixes the analysis window per rate: 512 samples at 16 kHz,
        // 256 at 8 kHz. Feeding 512 samples at 8 kHz mis-shapes the graph input
        // and the model throws or returns garbage.
        window_        = (cfg.sample_rate_hz == 16000) ? size_t{512} : size_t{256};
        buf_.reserve(window_ + frame_samples_);
        state_.assign(kStateDim, 0.0f);
        threshold_     = (cfg.vad_neural_threshold > 0.0f &&
                         cfg.vad_neural_threshold < 1.0f)
                            ? cfg.vad_neural_threshold
                            : 0.5f;
        // Hysteresis release threshold. Silero's reference VADIterator leaves
        // speech at threshold - 0.15; the gap stops boundary chatter from
        // chopping a single utterance into many micro-segments. Floored so an
        // aggressive (low) on-threshold still has a positive release point.
        neg_threshold_ = std::max(threshold_ - 0.15f, 0.05f);
        // Require two consecutive in-speech windows (~64 ms at 16 kHz) before
        // declaring speech, so a lone transient that momentarily crosses the
        // threshold does not open a spurious segment.
        onset_needed_  = 2;
        onset_count_   = 0;
        last_p_        = 0.0f;
        cached_active_ = false;
        return AURIS_OK;
    }

    DetectionResult process(const int16_t* frame, uint32_t n) override {
        DetectionResult r;
        if (session_ == nullptr || frame == nullptr || n == 0) return r;

        // Append the frame, converting int16 -> float in [-1, 1].
        const size_t old_size = buf_.size();
        buf_.resize(old_size + n);
        for (uint32_t i = 0; i < n; ++i) {
            buf_[old_size + i] =
                static_cast<float>(frame[i]) / 32768.0f;
        }

        // Drain windows until we cannot form another.
        while (buf_.size() >= window_) {
            cached_active_ = run_inference_(buf_.data());
            // Slide the buffer forward by one window. v1 keeps no overlap;
            // a refinement can use 50% overlap for tighter decisions.
            buf_.erase(buf_.begin(),
                       buf_.begin() + static_cast<std::ptrdiff_t>(window_));
        }

        r.active = cached_active_;
        // Surface the genuine model probability (not a binary 0/1) so the
        // host and the segment confidence carry real signal strength.
        r.confidence = std::min(1.0f, std::max(0.0f, last_p_));
        return r;
    }

private:
    bool run_inference_(const float* samples_512) {
        try {
            Ort::MemoryInfo mem =
                Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

            // input: [1, window_] (512 @ 16 kHz, 256 @ 8 kHz)
            const std::array<int64_t, 2> input_shape{1, static_cast<int64_t>(window_)};
            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                mem,
                const_cast<float*>(samples_512),
                window_,
                input_shape.data(),
                input_shape.size());

            // sr: scalar int64 (rank-0 tensor). Silero v5 expects a true
            // scalar; passing shape={1} works in some ORT versions but a
            // rank-1 tensor breaks the model's `gather` op on others.
            int64_t sr_val = static_cast<int64_t>(sample_rate_);
            Ort::Value sr_tensor = Ort::Value::CreateTensor<int64_t>(
                mem, &sr_val, 1, /*shape*/ nullptr, /*shape_len*/ 0);

            // state: [2, 1, 128]
            const std::array<int64_t, 3> state_shape{2, 1, 128};
            Ort::Value state_tensor = Ort::Value::CreateTensor<float>(
                mem,
                state_.data(),
                state_.size(),
                state_shape.data(),
                state_shape.size());

            std::array<const char*, 3> input_names {"input", "state", "sr"};
            // // VERIFY  Ort::Value has no default constructor in newer
            // ORT releases; use aggregate-initialization to skip the
            // default-construct + move-assign sequence std::array{}; ... }
            // would do otherwise.
            std::array<Ort::Value, 3> inputs {
                std::move(input_tensor),
                std::move(state_tensor),
                std::move(sr_tensor),
            };

            std::array<const char*, 2> output_names {"output", "stateN"};

            auto outputs = session_->Run(
                Ort::RunOptions{nullptr},
                input_names.data(),
                inputs.data(),
                inputs.size(),
                output_names.data(),
                output_names.size());

            if (outputs.size() != 2) return cached_active_;

            const float p_speech = *outputs[0].GetTensorData<float>();
            const size_t state_n_count =
                outputs[1].GetTensorTypeAndShapeInfo().GetElementCount();
            const float* new_state = outputs[1].GetTensorData<float>();
            const size_t copy_count = std::min(state_.size(), state_n_count);
            std::memcpy(state_.data(), new_state, copy_count * sizeof(float));

            last_p_ = p_speech;

            // Dual-threshold hysteresis with onset confirmation. Reads the
            // PREVIOUS decision (cached_active_) and returns the NEXT one; the
            // caller assigns it back to cached_active_.
            if (!cached_active_) {
                if (p_speech >= threshold_) {
                    if (++onset_count_ >= onset_needed_) {
                        onset_count_ = 0;
                        return true;
                    }
                } else {
                    onset_count_ = 0;
                }
                return false;
            }
            // Currently in speech: stay until probability drops below the
            // lower release threshold.
            if (p_speech < neg_threshold_) {
                onset_count_ = 0;
                return false;
            }
            return true;
        } catch (const Ort::Exception&) {
            return cached_active_;
        } catch (...) {
            return cached_active_;
        }
    }

    std::unique_ptr<Ort::Env>     env_;
    std::unique_ptr<Ort::Session> session_;
    std::vector<float>            buf_;
    std::vector<float>            state_;
    uint32_t                      sample_rate_   = 0;
    uint32_t                      frame_samples_ = 0;
    size_t                        window_        = kSileroWindow;  // per-rate (512@16k, 256@8k)
    float                         threshold_     = 0.5f;
    float                         neg_threshold_ = 0.35f;
    float                         last_p_        = 0.0f;
    int                           onset_count_   = 0;
    int                           onset_needed_  = 2;
    bool                          cached_active_ = false;
};

} // namespace

std::unique_ptr<Detector> make_neural_vad() {
    return std::make_unique<NeuralVadDetector>();
}

} // namespace auris

#else   // !AURIS_HAVE_NEURAL_VAD

namespace auris {
std::unique_ptr<Detector> make_neural_vad() { return nullptr; }
} // namespace auris

#endif
