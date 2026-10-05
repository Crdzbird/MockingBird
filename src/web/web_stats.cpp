// Stats record access for the WebAssembly shim.
#ifdef __EMSCRIPTEN__

#include "mockingbird/mockingbird.h"
#include "web/web_fields.hpp"

#include <emscripten/emscripten.h>

#include <cstdlib>

using namespace mockingbird::web;

extern "C" {

EMSCRIPTEN_KEEPALIVE mockingbird_stats_t* mockingbird_web_stats_new(void) {
    return static_cast<mockingbird_stats_t*>(std::calloc(1, sizeof(mockingbird_stats_t)));
}

EMSCRIPTEN_KEEPALIVE void mockingbird_web_stats_free(mockingbird_stats_t* s) { std::free(s); }

EMSCRIPTEN_KEEPALIVE int mockingbird_web_stats_field_count(void) { return kStatsFieldCount; }

EMSCRIPTEN_KEEPALIVE double mockingbird_web_stats_get(const mockingbird_stats_t* s, int field) {
    if (s == nullptr) return 0;
    switch (field) {
        case kFramesCaptured:              return static_cast<double>(s->frames_captured);
        case kFramesDroppedCapture:        return static_cast<double>(s->frames_dropped_capture);
        case kPacketsEmitted:              return static_cast<double>(s->packets_emitted);
        case kPacketsDropped:              return static_cast<double>(s->packets_dropped);
        case kSegmentsOpened:              return static_cast<double>(s->segments_opened);
        case kSegmentsClosed:              return static_cast<double>(s->segments_closed);
        case kPcmRingFillFrames:           return static_cast<double>(s->pcm_ring_fill_frames);
        case kPacketQueueFill:             return static_cast<double>(s->packet_queue_fill);
        case kInputRmsDbfs:                return static_cast<double>(s->input_rms_dbfs);
        case kInputPeakDbfs:               return static_cast<double>(s->input_peak_dbfs);
        case kTapSamplesDropped:           return static_cast<double>(s->tap_samples_dropped);
        case kWorkerBusyPpm:               return static_cast<double>(s->worker_busy_ppm);
        case kWorkerMaxFrameUs:            return static_cast<double>(s->worker_max_frame_us);
        case kWorkerWakeups:               return static_cast<double>(s->worker_wakeups);
        case kSoundClassInferences:        return static_cast<double>(s->sound_class_inferences);
        case kSoundClassInferencesSkipped: return static_cast<double>(s->sound_class_inferences_skipped);
        case kPreprocFramesSkipped:        return static_cast<double>(s->preproc_frames_skipped);
        case kDegradationLevel:            return static_cast<double>(s->degradation_level);
        default:                           return 0;
    }
}

} // extern "C"

#endif
