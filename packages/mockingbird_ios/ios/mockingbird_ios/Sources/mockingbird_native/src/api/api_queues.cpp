// The host-facing queues: events, packets, the PCM tap, and the callback.
#include "mockingbird/mockingbird.h"
#include "engine.hpp"

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_set_event_callback(mockingbird_engine_t* e, mockingbird_event_cb cb,
                                                  void* user) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->cb_mu);
        e->event_cb   = cb;
        e->event_user = user;
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_poll_event(mockingbird_engine_t* e, mockingbird_event_t* out) {
    if (e == nullptr || out == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->evt_mu);
        if (e->evt_queue.empty()) return MOCKINGBIRD_ERR_QUEUE_EMPTY;
        *out = e->evt_queue.front();
        e->evt_queue.pop_front();
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_drain(mockingbird_engine_t* e, mockingbird_packet_t* out) {
    if (e == nullptr || out == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        return e->packets.drain(out) ? MOCKINGBIRD_OK : MOCKINGBIRD_ERR_QUEUE_EMPTY;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_release(mockingbird_engine_t* e, const mockingbird_packet_t* pkt) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        return e->packets.release(pkt);
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_read_pcm(mockingbird_engine_t* e, int16_t* out,
                                        uint32_t max_samples, uint32_t* out_written) {
    if (e == nullptr || out == nullptr || out_written == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    *out_written = 0;
    if (!e->pcm_tap) return MOCKINGBIRD_ERR_UNSUPPORTED;
    if (max_samples == 0) return MOCKINGBIRD_OK;
    try {
        *out_written = static_cast<uint32_t>(
            e->pcm_tap->pop(out, static_cast<size_t>(max_samples)));
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_wait_handle(mockingbird_engine_t* e, intptr_t* out) {
    if (e == nullptr || out == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    return MOCKINGBIRD_ERR_UNSUPPORTED;
}

}
