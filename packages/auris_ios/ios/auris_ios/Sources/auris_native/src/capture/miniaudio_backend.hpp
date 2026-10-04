// Portable capture through miniaudio. The pimpl keeps the miniaudio header
// out of every other translation unit.
#pragma once

#include "capture/capture_backend.hpp"

#include <memory>

namespace auris {

class MiniaudioBackend final : public CaptureBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;

    auris_status_t init(const auris_config_t& cfg, PcmCallback cb, void* user) override;
    auris_status_t start() override;
    auris_status_t stop() override;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace auris
