// mockingbird_set_speaker_model, mockingbird_speaker_embedding and
// mockingbird_set_target_speaker.
#include "mockingbird/mockingbird.h"
#include "engine.hpp"

#include <algorithm>
#include <string>

namespace {
constexpr uint32_t kMaxDim = 4096;
}

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_set_speaker_model(mockingbird_engine_t* e, const char* path) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->lifecycle_mu);
        if (e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_INVALID_STATE;
        std::lock_guard sk(e->speaker.mu);
        e->speaker.model_path = path == nullptr ? std::string() : std::string(path);
        e->speaker.model.reset();
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_speaker_embedding(mockingbird_engine_t* e, const int16_t* samples,
                                                 uint32_t count, float* out, uint32_t capacity,
                                                 uint32_t* out_dim) {
    if (e == nullptr || samples == nullptr || out_dim == nullptr || (capacity > 0 && out == nullptr)) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    if (e->cfg.sample_rate_hz != 16000) return MOCKINGBIRD_ERR_UNSUPPORTED;
    if (count < mockingbird::SpeakerModel::kMinSamples) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        mockingbird_status_t status = MOCKINGBIRD_OK;
        const auto model = e->speaker.loaded(status);
        if (!model) return status;
        const std::vector<float> v = model->embed(samples, count);
        *out_dim = static_cast<uint32_t>(v.size());
        std::copy_n(v.begin(), std::min<size_t>(v.size(), capacity), out);
        return v.empty() ? MOCKINGBIRD_ERR_INTERNAL : MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_set_target_speaker(mockingbird_engine_t* e, const float* embedding,
                                                  uint32_t dim, float threshold) {
    if (e == nullptr || (embedding != nullptr && (dim == 0 || dim > kMaxDim))) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (!(threshold >= 0.0f && threshold < 1.0f)) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (embedding != nullptr && e->cfg.sample_rate_hz != 16000) return MOCKINGBIRD_ERR_UNSUPPORTED;
    try {
        std::lock_guard lk(e->speaker.mu);
        e->speaker.target.assign(embedding, embedding == nullptr ? embedding : embedding + dim);
        e->speaker.threshold = threshold > 0.0f ? threshold : 0.5f;
        e->speaker.active.store(embedding != nullptr, std::memory_order_release);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

}
