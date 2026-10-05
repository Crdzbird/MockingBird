// Capture backends produce int16 mono PCM on their own thread and hand it to
// the engine through an alloc-free callback.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>
#include <memory>

namespace mockingbird {

using PcmCallback = void (*)(void*          user,
                             const int16_t* samples,
                             uint32_t       frame_count,
                             uint64_t       capture_ts_us);

class CaptureBackend {
public:
    virtual ~CaptureBackend() = default;

    virtual mockingbird_status_t init(const mockingbird_config_t& cfg, PcmCallback cb, void* user) = 0;
    virtual mockingbird_status_t start() = 0;
    virtual mockingbird_status_t stop() = 0;
};

std::unique_ptr<CaptureBackend> make_backend(mockingbird_backend_t which);

using BackendFactoryFn = std::unique_ptr<CaptureBackend> (*)(mockingbird_backend_t);
void set_backend_factory_override(BackendFactoryFn fn);

} // namespace mockingbird
