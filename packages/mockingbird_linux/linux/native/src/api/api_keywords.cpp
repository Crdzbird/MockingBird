// mockingbird_set_keyword_frontend, mockingbird_add_keyword and
// mockingbird_clear_keywords. The worker reads the set when it starts.
#include "mockingbird/mockingbird.h"
#include "engine.hpp"

#include <string>

namespace {
constexpr size_t kMaxKeywords = 16;

std::string copy_or_empty(const char* s) { return s == nullptr ? std::string() : std::string(s); }

}

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_set_keyword_frontend(mockingbird_engine_t* e,
                                                    const char* melspectrogram_path,
                                                    const char* embedding_path) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->lifecycle_mu);
        if (e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_INVALID_STATE;
        e->keywords.melspectrogram_path = copy_or_empty(melspectrogram_path);
        e->keywords.embedding_path      = copy_or_empty(embedding_path);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_add_keyword(mockingbird_engine_t* e, const char* model_path,
                                           float threshold, uint32_t* out_id) {
    if (e == nullptr || model_path == nullptr || model_path[0] == '\0') return MOCKINGBIRD_ERR_INVALID_ARG;
    if (!(threshold >= 0.0f && threshold < 1.0f)) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->lifecycle_mu);
        if (e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_INVALID_STATE;
        if (e->keywords.keywords.size() >= kMaxKeywords) return MOCKINGBIRD_ERR_INVALID_ARG;
        e->keywords.keywords.push_back({std::string(model_path), threshold > 0.0f ? threshold : 0.5f});
        if (out_id != nullptr) *out_id = static_cast<uint32_t>(e->keywords.keywords.size() - 1);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_clear_keywords(mockingbird_engine_t* e) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->lifecycle_mu);
        if (e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_INVALID_STATE;
        e->keywords.keywords.clear();
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

}
