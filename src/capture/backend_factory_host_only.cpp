// Backend factory for builds with no capture device: only host-pushed PCM.
#ifdef __EMSCRIPTEN__

#include "capture/capture_backend.hpp"

namespace mockingbird {
namespace {
BackendFactoryFn g_backend_override = nullptr;
}

void set_backend_factory_override(BackendFactoryFn fn) {
    g_backend_override = fn;
}

std::unique_ptr<CaptureBackend> make_backend(mockingbird_backend_t which) {
    if (g_backend_override != nullptr) return g_backend_override(which);
    return nullptr;
}

mockingbird_status_t list_input_devices(mockingbird_backend_t which, std::vector<InputDevice>& out) {
    (void)which;
    out.clear();
    return MOCKINGBIRD_ERR_UNSUPPORTED;
}

}

#endif
