// src/capture/coreaudio_backend.hpp
//
// Native Apple Core Audio capture backend. Uses the Audio Unit API directly
// so we can pull OS host-time stamps off AudioTimeStamp.mHostTime instead of
// inferring them from steady_clock::now() like miniaudio does.
//
// Same source serves macOS and iOS; the only difference is the AudioUnit
// SubType (HALOutput vs RemoteIO), guarded by TARGET_OS_IOS.

#pragma once

#include "capture_backend.hpp"

#include <memory>

namespace auris {

#if defined(__APPLE__)

class CoreAudioBackend final : public CaptureBackend {
public:
    CoreAudioBackend();
    ~CoreAudioBackend() override;

    auris_status_t init(const auris_config_t& cfg,
                        PcmCallback           cb,
                        void*                 user) override;
    auris_status_t start() override;
    auris_status_t stop() override;

    // Pimpl so the AudioUnit / AudioToolbox includes do not leak into
    // every TU.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

#endif // __APPLE__

} // namespace auris
