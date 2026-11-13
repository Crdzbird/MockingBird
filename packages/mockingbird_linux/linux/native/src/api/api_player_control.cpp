// mockingbird_player_set_volume, _set_muted and _get_stats: atomics only,
// safe from any thread while the audio thread runs.
#include "mockingbird/mockingbird.h"
#include "play/player.hpp"

#include <cmath>

namespace {
uint32_t state_of(const mockingbird::PlayerShared& s) {
    if (!s.started.load(std::memory_order_acquire)) return MOCKINGBIRD_PLAYER_STOPPED;
    if (s.paused.load(std::memory_order_acquire) || s.route_paused.load(std::memory_order_acquire)) {
        return MOCKINGBIRD_PLAYER_PAUSED;
    }
    if (s.interrupted.load(std::memory_order_acquire)) return MOCKINGBIRD_PLAYER_INTERRUPTED;
    return s.render_state.load(std::memory_order_relaxed);
}

}

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_player_set_volume(mockingbird_player_t* p, float volume) {
    if (p == nullptr || !std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    p->shared.volume.store(volume, std::memory_order_relaxed);
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_set_muted(mockingbird_player_t* p, int muted) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    p->shared.muted.store(muted != 0, std::memory_order_relaxed);
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_get_stats(mockingbird_player_t* p,
                                                                  mockingbird_player_stats_t* out) {
    if (p == nullptr || out == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    const auto& s = p->shared;
    *out = mockingbird_player_stats_t{};
    out->packets_received  = s.received.load(std::memory_order_relaxed);
    out->packets_played    = s.played.load(std::memory_order_relaxed);
    out->packets_concealed = s.concealed.load(std::memory_order_relaxed);
    out->packets_dropped   = s.dropped.load(std::memory_order_relaxed);
    out->packets_late      = s.late.load(std::memory_order_relaxed);
    out->underruns         = s.underruns.load(std::memory_order_relaxed);
    out->frames_rendered   = s.frames_rendered.load(std::memory_order_relaxed);
    out->buffered_packets  = s.buffered_packets.load(std::memory_order_relaxed);
    out->buffered_ms       = s.buffered_ms.load(std::memory_order_relaxed);
    out->state             = state_of(s);
    out->output_restarts   = s.output_restarts.load(std::memory_order_relaxed);
    return MOCKINGBIRD_OK;
}

}
