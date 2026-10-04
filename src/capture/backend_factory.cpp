// Chooses the capture backend for a configured enum value. Tests may install
// an override factory to inject synthetic or failing backends.
#include "capture/capture_backend.hpp"
#include "capture/coreaudio_backend.hpp"
#include "capture/miniaudio_backend.hpp"
#include "capture/oboe_backend.hpp"

namespace auris {

namespace {
BackendFactoryFn g_backend_override = nullptr;
}

void set_backend_factory_override(BackendFactoryFn fn) {
    g_backend_override = fn;
}

std::unique_ptr<CaptureBackend> make_backend(auris_backend_t which) {
    if (g_backend_override != nullptr) return g_backend_override(which);
    switch (which) {
        case AURIS_BACKEND_AUTO:
        case AURIS_BACKEND_MINIAUDIO:
            return std::make_unique<MiniaudioBackend>();
        case AURIS_BACKEND_COREAUDIO:
#if defined(__APPLE__)
            return std::make_unique<CoreAudioBackend>();
#else
            return nullptr;
#endif
        case AURIS_BACKEND_OBOE:
#ifdef AURIS_HAVE_OBOE
            return std::make_unique<OboeBackend>();
#else
            return std::make_unique<MiniaudioBackend>();
#endif
        case AURIS_BACKEND_HOST_PCM:
        default:
            return nullptr;
    }
}

} // namespace auris
