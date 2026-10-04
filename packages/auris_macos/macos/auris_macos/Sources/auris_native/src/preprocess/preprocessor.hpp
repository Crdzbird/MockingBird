// Optional in-place cleaner that runs on the worker thread before the
// detectors and encoder see a frame.
#pragma once

#include "auris/auris.h"

#include <cstdint>
#include <memory>

namespace auris {

class Preprocessor {
public:
    virtual ~Preprocessor() = default;

    virtual auris_status_t init(const auris_config_t& cfg) = 0;
    virtual void process(int16_t* frame, uint32_t n) = 0;
};

std::unique_ptr<Preprocessor> make_rnnoise_preprocessor();

} // namespace auris
