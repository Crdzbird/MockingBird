// src/capture/oboe_backend.hpp
//
// Native Android capture via Oboe (Google's low-latency wrapper around
// AAudio + OpenSL ES). Compiled only when AURIS_HAVE_OBOE is defined; that
// macro is set by the Android plugin's CMake when find_package(oboe) succeeds.

#pragma once

#include "capture_backend.hpp"

#include <memory>

namespace auris {

#ifdef AURIS_HAVE_OBOE

class OboeBackend final : public CaptureBackend {
public:
    OboeBackend();
    ~OboeBackend() override;

    auris_status_t init(const auris_config_t& cfg,
                        PcmCallback           cb,
                        void*                 user) override;
    auris_status_t start() override;
    auris_status_t stop() override;

    // Pimpl keeps oboe/Oboe.h out of every TU.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

#endif // AURIS_HAVE_OBOE

} // namespace auris
