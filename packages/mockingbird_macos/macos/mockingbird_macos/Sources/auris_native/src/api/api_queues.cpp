// The host-facing queues: events, packets, the PCM tap, and the callback.
#include "auris/auris.h"
#include "engine.hpp"

extern "C" {

AURIS_API auris_status_t auris_set_event_callback(auris_engine_t* e, auris_event_cb cb,
                                                  void* user) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->cb_mu);
        e->event_cb   = cb;
        e->event_user = user;
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_poll_event(auris_engine_t* e, auris_event_t* out) {
    if (e == nullptr || out == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->evt_mu);
        if (e->evt_queue.empty()) return AURIS_ERR_QUEUE_EMPTY;
        *out = e->evt_queue.front();
        e->evt_queue.pop_front();
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_drain(auris_engine_t* e, auris_packet_t* out) {
    if (e == nullptr || out == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        return e->packets.drain(out) ? AURIS_OK : AURIS_ERR_QUEUE_EMPTY;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_release(auris_engine_t* e, const auris_packet_t* pkt) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        return e->packets.release(pkt);
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_read_pcm(auris_engine_t* e, int16_t* out,
                                        uint32_t max_samples, uint32_t* out_written) {
    if (e == nullptr || out == nullptr || out_written == nullptr) return AURIS_ERR_INVALID_ARG;
    *out_written = 0;
    if (!e->pcm_tap) return AURIS_ERR_UNSUPPORTED;
    if (max_samples == 0) return AURIS_OK;
    try {
        *out_written = static_cast<uint32_t>(
            e->pcm_tap->pop(out, static_cast<size_t>(max_samples)));
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_wait_handle(auris_engine_t* e, intptr_t* out) {
    if (e == nullptr || out == nullptr) return AURIS_ERR_INVALID_ARG;
    return AURIS_ERR_UNSUPPORTED;
}

} // extern "C"
