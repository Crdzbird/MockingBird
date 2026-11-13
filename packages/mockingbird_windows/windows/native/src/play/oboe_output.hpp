// Android playback through Oboe. Compiled only when the Android plugin's
// CMake defines MOCKINGBIRD_HAVE_OBOE.
#pragma once

#include "play/output_backend.hpp"

#include <memory>

namespace mockingbird {
#ifdef MOCKINGBIRD_HAVE_OBOE

class OboeOutput final : public OutputBackend {
public:
    OboeOutput();
    ~OboeOutput() override;

    mockingbird_status_t init(const OutputFormat& fmt, RenderCallback cb, void* user,
                              OutputNoticeCallback notice, void* notice_user) override;
    mockingbird_status_t start() override;
    mockingbird_status_t stop() override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

#endif

}
