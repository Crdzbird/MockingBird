// Android capture through Oboe. Compiled only when the Android plugin's
// CMake defines AURIS_HAVE_OBOE.
#pragma once

#include "capture/capture_backend.hpp"

#include <memory>

namespace auris {

#ifdef AURIS_HAVE_OBOE

class OboeBackend final : public CaptureBackend {
public:
    OboeBackend();
    ~OboeBackend() override;

    auris_status_t init(const auris_config_t& cfg, PcmCallback cb, void* user) override;
    auris_status_t start() override;
    auris_status_t stop() override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

#endif

} // namespace auris
