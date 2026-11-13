// Chooses the output backend for a configured enum value, as capture does:
// AUTO is Core Audio on Apple, Oboe on Android and miniaudio elsewhere.
// Tests may install an override to drive a synthetic audio thread.
#include "play/output_backend.hpp"

#include "play/coreaudio_output.hpp"
#include "play/miniaudio_output.hpp"
#include "play/oboe_output.hpp"

#include <atomic>

namespace mockingbird {
namespace {
std::atomic<OutputFactoryFn> g_override{nullptr};

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

void set_output_factory_override(OutputFactoryFn fn) {
    g_override.store(fn, std::memory_order_release);
}

std::unique_ptr<OutputBackend> make_output_backend(mockingbird_backend_t which) {
    const OutputFactoryFn fn = g_override.load(std::memory_order_acquire);
    if (fn != nullptr) return fn(which);
    switch (resolve(which)) {
#ifndef __EMSCRIPTEN__
        case MOCKINGBIRD_BACKEND_MINIAUDIO:
            return std::make_unique<MiniaudioOutput>();
        case MOCKINGBIRD_BACKEND_OBOE:
#ifdef MOCKINGBIRD_HAVE_OBOE
            return std::make_unique<OboeOutput>();
#else
            return std::make_unique<MiniaudioOutput>();
#endif
#endif
        case MOCKINGBIRD_BACKEND_COREAUDIO:
#if defined(__APPLE__)
            return std::make_unique<CoreAudioOutput>();
#else
            return nullptr;
#endif
        default:
            return nullptr;
    }
}

}
