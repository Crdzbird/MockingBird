// Native Apple playback through an output AudioUnit: RemoteIO on iOS, and on
// macOS the default output unit, which follows the system default device.
#pragma once

#include "play/output_backend.hpp"

#include <memory>

namespace mockingbird {
#if defined(__APPLE__)

class CoreAudioOutput final : public OutputBackend {
public:
    CoreAudioOutput();
    ~CoreAudioOutput() override;

    mockingbird_status_t init(const OutputFormat& fmt, RenderCallback cb, void* user,
                              OutputNoticeCallback notice, void* notice_user) override;
    mockingbird_status_t start() override;
    mockingbird_status_t stop() override;

    struct Impl;

private:
    std::shared_ptr<Impl> impl_;
};

#endif

}
