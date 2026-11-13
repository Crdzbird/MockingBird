// mockingbird_player_start, _pause, _resume and _stop. The lifecycle mutex
// serialises them; the audio thread never takes it.
#include "mockingbird/mockingbird.h"
#include "play/player.hpp"

namespace {
mockingbird_status_t start_output(mockingbird_player* p) {
    if (p->cfg.backend == MOCKINGBIRD_BACKEND_HOST_PCM) return MOCKINGBIRD_OK;
    const mockingbird_status_t s = p->output->start();
    return s == MOCKINGBIRD_ERR_ALREADY_RUNNING ? MOCKINGBIRD_OK : s;
}

void stop_output(mockingbird_player* p) {
    if (p->output) p->output->stop();
}

}

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_player_start(mockingbird_player_t* p) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(p->lifecycle_mu);
        if (p->shared.started.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
        mockingbird_status_t s = mockingbird::open_player_output(p);
        if (s == MOCKINGBIRD_OK) s = start_output(p);
        if (s != MOCKINGBIRD_OK) return s;
        p->shared.paused.store(false, std::memory_order_release);
        p->shared.route_paused.store(false, std::memory_order_release);
        p->shared.interrupted.store(false, std::memory_order_release);
        p->shared.started.store(true, std::memory_order_release);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_pause(mockingbird_player_t* p) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    std::lock_guard lk(p->lifecycle_mu);
    if (!p->shared.started.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_NOT_RUNNING;
    if (p->shared.paused.exchange(true, std::memory_order_acq_rel)) return MOCKINGBIRD_OK;
    stop_output(p);
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_resume(mockingbird_player_t* p) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    std::lock_guard lk(p->lifecycle_mu);
    if (!p->shared.started.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_NOT_RUNNING;
    const mockingbird_status_t s = start_output(p);
    if (s != MOCKINGBIRD_OK) return s;
    p->shared.paused.store(false, std::memory_order_release);
    p->shared.route_paused.store(false, std::memory_order_release);
    p->shared.interrupted.store(false, std::memory_order_release);
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_stop(mockingbird_player_t* p) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    std::lock_guard lk(p->lifecycle_mu);
    if (!p->shared.started.exchange(false, std::memory_order_acq_rel)) return MOCKINGBIRD_OK;
    stop_output(p);
    p->shared.generation.fetch_add(1, std::memory_order_acq_rel);
    p->shared.paused.store(false, std::memory_order_release);
    p->shared.route_paused.store(false, std::memory_order_release);
    p->shared.interrupted.store(false, std::memory_order_release);
    return MOCKINGBIRD_OK;
}

}
