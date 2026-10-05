// Optional in-place cleaner that runs on the worker thread before the
// detectors and encoder see a frame.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>
#include <memory>

namespace mockingbird {

class Preprocessor {
public:
    virtual ~Preprocessor() = default;

    virtual mockingbird_status_t init(const mockingbird_config_t& cfg) = 0;
    virtual void process(int16_t* frame, uint32_t n) = 0;
};

std::unique_ptr<Preprocessor> make_rnnoise_preprocessor();

} // namespace mockingbird
