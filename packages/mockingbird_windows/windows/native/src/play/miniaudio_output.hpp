// Portable playback through miniaudio on the default output device, which
// miniaudio follows when the system default changes. The pimpl keeps the
// miniaudio header out of every other translation unit.
#pragma once

#include "play/output_backend.hpp"

#include <memory>

namespace mockingbird {
class MiniaudioOutput final : public OutputBackend {
public:
    MiniaudioOutput();
    ~MiniaudioOutput() override;

    mockingbird_status_t init(const OutputFormat& fmt, RenderCallback cb, void* user,
                              OutputNoticeCallback notice, void* notice_user) override;
    mockingbird_status_t start() override;
    mockingbird_status_t stop() override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}
