// Packet record access for the WebAssembly shim. The data field is returned
// as the payload's address in linear memory.
#ifdef __EMSCRIPTEN__

#include "mockingbird/mockingbird.h"
#include "web/web_fields.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cstdlib>

using namespace mockingbird::web;

extern "C" {

EMSCRIPTEN_KEEPALIVE mockingbird_packet_t* mockingbird_web_packet_new(void) {
    return static_cast<mockingbird_packet_t*>(std::calloc(1, sizeof(mockingbird_packet_t)));
}

EMSCRIPTEN_KEEPALIVE void mockingbird_web_packet_free(mockingbird_packet_t* p) { std::free(p); }

EMSCRIPTEN_KEEPALIVE int mockingbird_web_packet_field_count(void) { return kPacketFieldCount; }

EMSCRIPTEN_KEEPALIVE double mockingbird_web_packet_get(const mockingbird_packet_t* p, int field) {
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
