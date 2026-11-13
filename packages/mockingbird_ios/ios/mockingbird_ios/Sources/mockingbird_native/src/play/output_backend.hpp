// Output backends pull int16 PCM from the player on the OS audio thread
// through an alloc-free callback, and tell it what the OS did to the output:
// an interruption and its end, a route it followed, or a route that went away.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>
#include <memory>

namespace mockingbird {
enum class OutputNotice { Interrupted, Resumed, RouteChanged, RouteLost };

using RenderCallback       = void (*)(void* user, int16_t* out, uint32_t frames);
using OutputNoticeCallback = void (*)(void* user, OutputNotice notice);

struct OutputFormat {
    uint32_t sample_rate_hz = 16000;
    uint16_t channels       = 1;
    uint32_t period_frames  = 160;
};

class OutputBackend {
public:
    virtual ~OutputBackend() = default;

    virtual mockingbird_status_t init(const OutputFormat& fmt, RenderCallback cb, void* user,
                                      OutputNoticeCallback notice, void* notice_user) = 0;
    virtual mockingbird_status_t start() = 0;
    virtual mockingbird_status_t stop() = 0;
};

std::unique_ptr<OutputBackend> make_output_backend(mockingbird_backend_t which);

using OutputFactoryFn = std::unique_ptr<OutputBackend> (*)(mockingbird_backend_t);
void set_output_factory_override(OutputFactoryFn fn);

}
