// Event delivery: the bounded pull queue, the optional push callback, and
// the coalesced wake that tells a pulling host to drain.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>

struct mockingbird_engine;

namespace mockingbird {
mockingbird_event_t make_event(mockingbird_event_type_t type,
                         uint64_t           segment_id,
                         uint64_t           timestamp_us,
                         uint32_t           reason_mask = 0,
                         float              confidence  = 0.0f);

void emit_event(mockingbird_engine* e, const mockingbird_event_t& evt);

void emit_backend_error(mockingbird_engine* e, mockingbird_status_t status);

void notify_host(mockingbird_engine* e);

}
