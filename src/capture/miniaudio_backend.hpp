// src/capture/miniaudio_backend.hpp
//
// // ASSUMES  miniaudio.h is vendored under third_party/miniaudio and its
// implementation lives in a single dedicated TU (miniaudio_impl.c). This
// header is pimpl so callers do not transitively include the 3.9 MB
// miniaudio header.

#pragma once

#include "capture_backend.hpp"

#include <memory>

namespace auris {

class MiniaudioBackend final : public CaptureBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;

    auris_status_t init(const auris_config_t& cfg,
                        PcmCallback           cb,
                        void*                 user) override;
    auris_status_t start() override;
    auris_status_t stop() override;

    // Public so the C data callback (free function) can reach the state.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace auris
