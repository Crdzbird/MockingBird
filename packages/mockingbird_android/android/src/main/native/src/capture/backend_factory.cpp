// Chooses the capture backend for a configured enum value; AUTO is Core
// Audio on Apple, Oboe on Android and miniaudio elsewhere. Tests may install
// an override factory to inject synthetic or failing backends.
#ifndef __EMSCRIPTEN__

#include "capture/capture_backend.hpp"
#include "capture/coreaudio_backend.hpp"
#include "capture/miniaudio_backend.hpp"
#include "capture/oboe_backend.hpp"

namespace mockingbird {
namespace {
BackendFactoryFn g_backend_override = nullptr;

mockingbird_backend_t resolve(mockingbird_backend_t which) {
    if (which != MOCKINGBIRD_BACKEND_AUTO) return which;
#if defined(__APPLE__)
    return MOCKINGBIRD_BACKEND_COREAUDIO;
#elif defined(MOCKINGBIRD_HAVE_OBOE)
    return MOCKINGBIRD_BACKEND_OBOE;
#else
    return MOCKINGBIRD_BACKEND_MINIAUDIO;
#endif
}
}

void set_backend_factory_override(BackendFactoryFn fn) {
    g_backend_override = fn;
}

std::unique_ptr<CaptureBackend> make_backend(mockingbird_backend_t which) {
    if (g_backend_override != nullptr) return g_backend_override(which);
    switch (resolve(which)) {
        case MOCKINGBIRD_BACKEND_MINIAUDIO:
            return std::make_unique<MiniaudioBackend>();
        case MOCKINGBIRD_BACKEND_COREAUDIO:
#if defined(__APPLE__)
            return std::make_unique<CoreAudioBackend>();
#else
            return nullptr;
#endif
        case MOCKINGBIRD_BACKEND_OBOE:
#ifdef MOCKINGBIRD_HAVE_OBOE
            return std::make_unique<OboeBackend>();
#else
            return std::make_unique<MiniaudioBackend>();
#endif
        case MOCKINGBIRD_BACKEND_HOST_PCM:
        default:
            return nullptr;
    }
}

mockingbird_status_t list_input_devices(mockingbird_backend_t which, std::vector<InputDevice>& out) {
    switch (resolve(which)) {
        case MOCKINGBIRD_BACKEND_MINIAUDIO:
            return miniaudio_list_input_devices(out);
        case MOCKINGBIRD_BACKEND_COREAUDIO:
#if defined(__APPLE__)
            return coreaudio_list_input_devices(out);
#else
            return MOCKINGBIRD_ERR_UNSUPPORTED;
#endif
        case MOCKINGBIRD_BACKEND_OBOE:
        case MOCKINGBIRD_BACKEND_HOST_PCM:
        default:
            return MOCKINGBIRD_ERR_UNSUPPORTED;
    }
}

}

#endif
