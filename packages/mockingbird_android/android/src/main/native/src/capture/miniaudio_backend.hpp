// Portable capture through miniaudio. The pimpl keeps the miniaudio header
// out of every other translation unit.
#pragma once

#include "capture/capture_backend.hpp"

#include <memory>

namespace mockingbird {
class MiniaudioBackend final : public CaptureBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;

    mockingbird_status_t init(const mockingbird_config_t& cfg, PcmCallback cb, void* user) override;
    mockingbird_status_t start() override;
    mockingbird_status_t stop() override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}
