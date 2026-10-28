// mockingbird_get_stats: relaxed loads of the worker's counters.
#include "mockingbird/mockingbird.h"
#include "engine.hpp"

#include <cstring>

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_get_stats(mockingbird_engine_t* e, mockingbird_stats_t* out) {
    if (e == nullptr || out == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        constexpr auto relaxed = std::memory_order_relaxed;
        out->frames_captured                = e->frames_captured.load(relaxed);
        out->frames_dropped_capture         = e->frames_dropped_capture.load(relaxed);
        out->packets_emitted                = e->packets_emitted.load(relaxed);
        out->packets_dropped                = e->packets_dropped.load(relaxed);
        out->segments_opened                = e->segments_opened.load(relaxed);
        out->segments_closed                = e->segments_closed.load(relaxed);
        out->pcm_ring_fill_frames           = static_cast<uint32_t>(e->pcm_ring.size_relaxed());
        out->packet_queue_fill              = static_cast<uint32_t>(e->packets.ready_size());
        out->input_rms_dbfs                 = e->input_rms_dbfs.load(relaxed);
        out->input_peak_dbfs                = e->input_peak_dbfs.load(relaxed);
        out->tap_samples_dropped            = e->tap_samples_dropped.load(relaxed);
        out->worker_busy_ppm                = e->worker_busy_ppm.load(relaxed);
        out->worker_max_frame_us            = e->worker_max_frame_us.load(relaxed);
        out->worker_wakeups                 = e->worker_wakeups.load(relaxed);
        out->sound_class_inferences         = e->sound_class_inferences.load(relaxed);
        out->sound_class_inferences_skipped = e->sound_class_inferences_skipped.load(relaxed);
        out->preproc_frames_skipped         = e->preproc_frames_skipped.load(relaxed);
        out->degradation_level              = e->degradation_level.load(relaxed);
        std::memset(out->_reserved, 0, sizeof(out->_reserved));
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

}
