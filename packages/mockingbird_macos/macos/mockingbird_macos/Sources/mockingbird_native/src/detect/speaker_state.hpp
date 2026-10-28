// What the speaker verification C API sets and the worker reads: the model,
// loaded on first use and shared, and the target voice print with its
// threshold. Everything but `active` is guarded by `mu`.
#pragma once

#include "detect/speaker_model.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mockingbird {
struct SpeakerState {
    std::mutex                    mu;
    std::string                   model_path;
    std::shared_ptr<SpeakerModel> model;
    std::vector<float>            target;
    float                         threshold = 0.5f;
    std::atomic<bool>             active{false};

    std::shared_ptr<SpeakerModel> loaded(mockingbird_status_t& status) {
        std::lock_guard lk(mu);
        status = MOCKINGBIRD_OK;
        if (!model) model = SpeakerModel::load(model_path, status);
        return model;
    }
};

}
