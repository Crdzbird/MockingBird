// Android capture through Oboe. Compiled only when the Android plugin's
// CMake defines MOCKINGBIRD_HAVE_OBOE.
#pragma once

#include "capture/capture_backend.hpp"

#include <memory>

namespace mockingbird {
#ifdef MOCKINGBIRD_HAVE_OBOE

class OboeBackend final : public CaptureBackend {
public:
    OboeBackend();
    ~OboeBackend() override;

    mockingbird_status_t init(const mockingbird_config_t& cfg, PcmCallback cb, void* user) override;
    mockingbird_status_t start() override;
    mockingbird_status_t stop() override;
    void set_notice_callback(CaptureNoticeCallback cb, void* user) override;
    mockingbird_status_t select_device(const std::string& id) override;
    uint32_t voice_processing_active() const override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

#endif

}
