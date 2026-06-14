// src/capture/capture_backend.hpp
//
// Abstract capture backend interface. Backends produce int16 mono PCM on
// their own thread (T1) and feed it through the supplied callback.
//
// // CONTRACT  The callback runs on the backend's RT-ish thread. It must be
// alloc-free and never block on the host. Implementations of the callback
// (currently the engine) limit themselves to atomic ops and a lock-free
// ring push.

#pragma once

#include "auris/auris.h"

#include <cstdint>
#include <memory>

namespace auris {

// Plain function pointer, not std::function, to keep the hot path branchless
// and free of any small-object-buffer indirection.
using PcmCallback = void (*)(void*          user,
                             const int16_t* samples,
                             uint32_t       frame_count,
                             uint64_t       capture_ts_us);

class CaptureBackend {
public:
    virtual ~CaptureBackend() = default;

    // Prepare the device. Returns AURIS_ERR_PERMISSION if the OS denied mic
    // access, AURIS_ERR_BACKEND on any other backend failure.
    virtual auris_status_t init(const auris_config_t& cfg,
                                PcmCallback           cb,
                                void*                 user) = 0;

    // Begin streaming. Idempotent: AURIS_ERR_ALREADY_RUNNING if already started.
    virtual auris_status_t start() = 0;

    // Stop streaming. Blocks until the data callback has finished firing.
    // Idempotent on a not-started backend.
    virtual auris_status_t stop() = 0;
};

// Factory. Returns nullptr for AURIS_BACKEND_HOST_PCM (no internal capture).
// AUTO + MINIAUDIO + (for now) COREAUDIO + OBOE all map to MiniaudioBackend;
// native Core Audio and native Oboe slot in transparently in step 2.5.
std::unique_ptr<CaptureBackend> make_backend(auris_backend_t which);

// // TEST-SEAM  When set, make_backend() delegates to this factory instead of
// constructing a real device backend, letting tests inject mock backends that
// fail at init/start or feed synthetic PCM. NOT part of the public C ABI. Pass
// nullptr to restore the default factory. Intended for single-threaded use:
// set it, exercise auris_create/start, then reset it.
using BackendFactoryFn = std::unique_ptr<CaptureBackend> (*)(auris_backend_t);
void set_backend_factory_override(BackendFactoryFn fn);

} // namespace auris
