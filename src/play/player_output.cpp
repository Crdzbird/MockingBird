// Opens the player's output on first start and routes the backend's audio
// thread and notices into the player. The render trampoline is the only code
// the OS audio thread runs.
#include "play/player.hpp"

namespace mockingbird {
namespace {
void render_trampoline(void* user, int16_t* out, uint32_t frames) {
    static_cast<mockingbird_player*>(user)->renderer.render(out, frames);
}

void notice_trampoline(void* user, OutputNotice notice) {
    auto& shared = static_cast<mockingbird_player*>(user)->shared;
    switch (notice) {
        case OutputNotice::Interrupted:
            shared.interrupted.store(true, std::memory_order_release);
            return;
        case OutputNotice::Resumed:
            shared.interrupted.store(false, std::memory_order_release);
            return;
        case OutputNotice::RouteChanged:
            shared.output_restarts.fetch_add(1, std::memory_order_relaxed);
            return;
        case OutputNotice::RouteLost:
            shared.route_paused.store(true, std::memory_order_release);
            return;
    }
}

}

mockingbird_status_t open_player_output(mockingbird_player* p) {
    if (p->cfg.backend == MOCKINGBIRD_BACKEND_HOST_PCM || p->output) return MOCKINGBIRD_OK;
    auto out = make_output_backend(p->cfg.backend);
    if (!out) return MOCKINGBIRD_ERR_UNSUPPORTED;
    OutputFormat fmt;
    fmt.sample_rate_hz = p->cfg.sample_rate_hz;
    fmt.channels       = p->cfg.channels;
    fmt.period_frames  = p->cfg.sample_rate_hz / 100u;
    const mockingbird_status_t s = out->init(fmt, render_trampoline, p, notice_trampoline, p);
    if (s != MOCKINGBIRD_OK) return s;
    p->output = std::move(out);
    return MOCKINGBIRD_OK;
}

}
