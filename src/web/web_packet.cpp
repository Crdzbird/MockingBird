// Packet record access for the WebAssembly shim. The data field is returned
// as the payload's address in linear memory.
#ifdef __EMSCRIPTEN__

#include "auris/auris.h"
#include "web/web_fields.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cstdlib>

using namespace auris::web;

extern "C" {

EMSCRIPTEN_KEEPALIVE auris_packet_t* auris_web_packet_new(void) {
    return static_cast<auris_packet_t*>(std::calloc(1, sizeof(auris_packet_t)));
}

EMSCRIPTEN_KEEPALIVE void auris_web_packet_free(auris_packet_t* p) { std::free(p); }

EMSCRIPTEN_KEEPALIVE int auris_web_packet_field_count(void) { return kPacketFieldCount; }

EMSCRIPTEN_KEEPALIVE double auris_web_packet_get(const auris_packet_t* p, int field) {
    if (p == nullptr) return 0;
    switch (field) {
        case kPacketSegmentId:   return static_cast<double>(p->segment_id);
        case kPacketPtsUs:       return static_cast<double>(p->pts_us);
        case kPacketCaptureTsUs: return static_cast<double>(p->capture_ts_us);
        case kPacketFlags:       return static_cast<double>(p->flags);
        case kPacketCodec:       return static_cast<double>(p->codec);
        case kPacketSize:        return static_cast<double>(p->size);
        case kPacketData:        return static_cast<double>(reinterpret_cast<uintptr_t>(p->data));
        default:                 return 0;
    }
}

} // extern "C"

#endif
