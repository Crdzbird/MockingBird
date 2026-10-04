// One Silero inference: the audio window, the recurrent state and a scalar
// sample rate in; the speech probability and the next state out.
#include "detect/neural_vad_detector.hpp"

#ifdef AURIS_HAVE_NEURAL_VAD

#include <algorithm>
#include <array>
#include <cstring>

namespace auris {

bool NeuralVadDetector::decide(float p_speech) {
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
    if (p_speech < neg_threshold_) {
        onset_count_ = 0;
        return false;
    }
    return true;
}

bool NeuralVadDetector::run_inference(const float* window) {
    try {
        std::copy(context_.begin(), context_.end(), input_.begin());
        std::copy(window, window + window_, input_.begin() + static_cast<std::ptrdiff_t>(context_.size()));
        std::copy(input_.end() - static_cast<std::ptrdiff_t>(context_.size()), input_.end(),
                  context_.begin());
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 2> input_shape{1, static_cast<int64_t>(input_.size())};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            mem, input_.data(), input_.size(), input_shape.data(), input_shape.size());
        int64_t    sr_val    = static_cast<int64_t>(sample_rate_);
        Ort::Value sr_tensor = Ort::Value::CreateTensor<int64_t>(mem, &sr_val, 1, nullptr, 0);
        const std::array<int64_t, 3> state_shape{2, 1, 128};
        Ort::Value state_tensor = Ort::Value::CreateTensor<float>(
            mem, state_.data(), state_.size(), state_shape.data(), state_shape.size());

        std::array<const char*, 3> input_names{"input", "state", "sr"};
        std::array<Ort::Value, 3>  inputs{std::move(input_tensor), std::move(state_tensor),
                                         std::move(sr_tensor)};
        std::array<const char*, 2> output_names{"output", "stateN"};
        auto outputs = session_->Run(Ort::RunOptions{nullptr}, input_names.data(), inputs.data(),
                                     inputs.size(), output_names.data(), output_names.size());
        if (outputs.size() != 2) return cached_active_;

        const float  p_speech   = *outputs[0].GetTensorData<float>();
        const size_t state_n    = outputs[1].GetTensorTypeAndShapeInfo().GetElementCount();
        const float* new_state  = outputs[1].GetTensorData<float>();
        const size_t copy_count = std::min(state_.size(), state_n);
        std::memcpy(state_.data(), new_state, copy_count * sizeof(float));
        last_p_ = p_speech;
        return decide(p_speech);
    } catch (...) {
        return cached_active_;
    }
}

} // namespace auris

#endif
