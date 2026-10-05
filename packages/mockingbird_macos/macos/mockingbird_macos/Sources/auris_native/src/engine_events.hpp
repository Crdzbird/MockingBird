// Event delivery: the bounded pull queue plus the optional push callback.
#pragma once

#include "auris/auris.h"

#include <cstdint>

struct auris_engine;

namespace auris {

auris_event_t make_event(auris_event_type_t type,
                         uint64_t           segment_id,
                         uint64_t           timestamp_us,
                         uint32_t           reason_mask = 0,
                         float              confidence  = 0.0f);

void emit_event(auris_engine* e, const auris_event_t& evt);

void emit_backend_error(auris_engine* e, auris_status_t status);

} // namespace auris
