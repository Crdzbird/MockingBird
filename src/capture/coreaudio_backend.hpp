// Native Apple capture through an AudioUnit, which exposes the OS host-time
// stamp of each buffer. Serves both macOS (HALOutput) and iOS (RemoteIO).
#pragma once

#include "capture/capture_backend.hpp"

#include <memory>

namespace auris {

#if defined(__APPLE__)

class CoreAudioBackend final : public CaptureBackend {
public:
    CoreAudioBackend();
    ~CoreAudioBackend() override;

    auris_status_t init(const auris_config_t& cfg, PcmCallback cb, void* user) override;
    auris_status_t start() override;
    auris_status_t stop() override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

#endif

} // namespace auris
