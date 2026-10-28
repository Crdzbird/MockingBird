// Capture backends produce int16 mono PCM on their own thread and hand it to
// the engine through an alloc-free callback. A second callback reports what
// the operating system did to capture: an interruption, its end, or a route
// change the backend recovered from.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mockingbird {
using PcmCallback = void (*)(void*          user,
                             const int16_t* samples,
                             uint32_t       frame_count,
                             uint64_t       capture_ts_us);

using CaptureNoticeCallback = void (*)(void*                    user,
                                       mockingbird_event_type_t type,
                                       uint32_t                 reason);

struct InputDevice {
    std::string id;
    std::string name;
    bool        is_default = false;
};

class CaptureBackend {
public:
    virtual ~CaptureBackend() = default;

    virtual mockingbird_status_t init(const mockingbird_config_t& cfg, PcmCallback cb, void* user) = 0;
    virtual mockingbird_status_t start() = 0;
    virtual mockingbird_status_t stop() = 0;

    virtual void set_notice_callback(CaptureNoticeCallback cb, void* user) {
        (void)cb;
        (void)user;
    }

    virtual mockingbird_status_t select_device(const std::string& id) {
        return id.empty() ? MOCKINGBIRD_OK : MOCKINGBIRD_ERR_UNSUPPORTED;
    }

    virtual uint32_t voice_processing_active() const { return 0; }
};

std::unique_ptr<CaptureBackend> make_backend(mockingbird_backend_t which);

mockingbird_status_t list_input_devices(mockingbird_backend_t which, std::vector<InputDevice>& out);

using BackendFactoryFn = std::unique_ptr<CaptureBackend> (*)(mockingbird_backend_t);
void set_backend_factory_override(BackendFactoryFn fn);

}
