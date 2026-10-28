// A push-only host never drains the pull queue, so it is capped and drops
// the oldest entry, the same policy as the packet queue.
#include "engine_events.hpp"

#include "engine.hpp"

namespace mockingbird {
namespace {
constexpr size_t kPullEventQueueCap = 1024;
}

mockingbird_event_t make_event(mockingbird_event_type_t type, uint64_t segment_id,
                         uint64_t timestamp_us, uint32_t reason_mask,
                         float confidence) {
    mockingbird_event_t evt{};
    evt.type         = type;
    evt.segment_id   = segment_id;
    evt.timestamp_us = timestamp_us;
    evt.reason_mask  = reason_mask;
    evt.confidence   = confidence;
    return evt;
}

void emit_event(mockingbird_engine* e, const mockingbird_event_t& evt) {
    {
        std::lock_guard lk(e->evt_mu);
        while (e->evt_queue.size() >= kPullEventQueueCap) {
            e->evt_queue.pop_front();
        }
        e->evt_queue.push_back(evt);
    }
    mockingbird_event_cb cb;
    void*          user;
    {
        std::lock_guard lk(e->cb_mu);
        cb   = e->event_cb;
        user = e->event_user;
    }
    if (cb == nullptr) return;
    try {
        cb(&evt, user);
    } catch (...) {
    }
}

void emit_backend_error(mockingbird_engine* e, mockingbird_status_t status) {
    emit_event(e, make_event(MOCKINGBIRD_EVT_BACKEND_ERROR, 0,
                             e->last_capture_ts_us.load(std::memory_order_relaxed),
                             static_cast<uint32_t>(status)));
}

}
