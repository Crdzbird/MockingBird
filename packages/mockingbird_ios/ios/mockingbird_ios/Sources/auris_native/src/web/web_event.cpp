// Event record access for the WebAssembly shim.
#ifdef __EMSCRIPTEN__

#include "auris/auris.h"
#include "web/web_fields.hpp"

#include <emscripten/emscripten.h>

#include <cstdlib>

using namespace auris::web;

extern "C" {

EMSCRIPTEN_KEEPALIVE auris_event_t* auris_web_event_new(void) {
    return static_cast<auris_event_t*>(std::calloc(1, sizeof(auris_event_t)));
}

EMSCRIPTEN_KEEPALIVE void auris_web_event_free(auris_event_t* e) { std::free(e); }

EMSCRIPTEN_KEEPALIVE int auris_web_event_field_count(void) { return kEventFieldCount; }

EMSCRIPTEN_KEEPALIVE double auris_web_event_get(const auris_event_t* e, int field) {
    if (e == nullptr) return 0;
    switch (field) {
        case kEventType:        return static_cast<double>(e->type);
        case kEventSegmentId:   return static_cast<double>(e->segment_id);
        case kEventTimestampUs: return static_cast<double>(e->timestamp_us);
        case kEventReasonMask:  return static_cast<double>(e->reason_mask);
        case kEventConfidence:  return static_cast<double>(e->confidence);
        default:                return 0;
    }
}

} // extern "C"

#endif
