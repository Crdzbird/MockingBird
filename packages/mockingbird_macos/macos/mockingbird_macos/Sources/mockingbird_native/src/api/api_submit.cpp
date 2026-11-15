// mockingbird_submit and mockingbird_player_submit: the blocking lifecycle
// calls run on the engine's or the player's own lifecycle thread, in order,
// with the result reported through the caller's done callback, or posted
// to the caller's port.
#include "mockingbird/mockingbird.h"
#include "config_validation.hpp"
#include "engine.hpp"
#include "capture/capture_backend.hpp"
#include "host_post.hpp"
#include "play/player.hpp"

#include <mutex>

namespace {
mockingbird_status_t stopped(mockingbird_status_t s) {
    return s == MOCKINGBIRD_ERR_NOT_RUNNING ? MOCKINGBIRD_OK : s;
}

mockingbird_status_t close_engine(mockingbird_engine_t* e) {
    const mockingbird_status_t s = stopped(mockingbird_stop(e));
    if (s != MOCKINGBIRD_OK) return s;
    std::lock_guard lk(e->lifecycle_mu);
    if (e->capture_backend && e->capture_backend_used) {
        e->capture_backend      = mockingbird::make_backend(e->cfg.backend);
        e->capture_backend_used = false;
        if (!e->capture_backend) return MOCKINGBIRD_ERR_UNSUPPORTED;
    }
    return MOCKINGBIRD_OK;
}

mockingbird_status_t close_player(mockingbird_player_t* p) {
    const mockingbird_status_t s = stopped(mockingbird_player_stop(p));
    if (s != MOCKINGBIRD_OK) return s;
    std::lock_guard lk(p->lifecycle_mu);
    p->output.reset();
    return MOCKINGBIRD_OK;
}

mockingbird_status_t run_engine_op(mockingbird_engine_t* e, uint32_t op) {
    switch (op) {
        case MOCKINGBIRD_ENGINE_OP_START:
            return mockingbird_start(e);
        case MOCKINGBIRD_ENGINE_OP_STOP:
            return mockingbird_stop(e);
        default:
            return close_engine(e);
    }
}

mockingbird_status_t run_player_op(mockingbird_player_t* p, uint32_t op) {
    switch (op) {
        case MOCKINGBIRD_PLAYER_OP_START:
            return mockingbird_player_start(p);
        case MOCKINGBIRD_PLAYER_OP_PAUSE:
            return mockingbird_player_pause(p);
        case MOCKINGBIRD_PLAYER_OP_RESUME:
            return mockingbird_player_resume(p);
        case MOCKINGBIRD_PLAYER_OP_STOP:
            return mockingbird_player_stop(p);
        default:
            return close_player(p);
    }
}

template <typename Target, typename Run>
mockingbird_status_t submit(mockingbird::LifecycleThread& thread, Target* target, uint32_t op, Run run,
                            mockingbird_done_cb done, void* user) {
    return thread.post([target, op, run, done, user] {
        mockingbird_status_t s = MOCKINGBIRD_ERR_INTERNAL;
        try {
            s = run(target, op);
        } catch (...) {
        }
        if (done != nullptr) done(user, op, static_cast<int32_t>(s));
    });
}

template <typename Target, typename Run>
mockingbird_status_t submit_port(mockingbird::LifecycleThread& thread, Target* target, uint32_t op,
                                 Run run, mockingbird_post_cb post, int64_t port) {
    return thread.post([target, op, run, post, port] {
        mockingbird_status_t s = MOCKINGBIRD_ERR_INTERNAL;
        try {
            s = run(target, op);
        } catch (...) {
        }
        mockingbird::post_done(post, port, op, static_cast<int32_t>(s));
    });
}

int engine_op(mockingbird_engine_op_t op) {
    const int kind = mockingbird::enum_as_int(op);
    return kind < MOCKINGBIRD_ENGINE_OP_START || kind > MOCKINGBIRD_ENGINE_OP_CLOSE ? -1 : kind;
}

int player_op(mockingbird_player_op_t op) {
    const int kind = mockingbird::enum_as_int(op);
    return kind < MOCKINGBIRD_PLAYER_OP_START || kind > MOCKINGBIRD_PLAYER_OP_CLOSE ? -1 : kind;
}

}

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_submit(mockingbird_engine_t* e, mockingbird_engine_op_t op,
                                                        mockingbird_done_cb done, void* user) {
    const int kind = engine_op(op);
    if (e == nullptr || kind < 0) return MOCKINGBIRD_ERR_INVALID_ARG;
    return submit(e->lifecycle, e, static_cast<uint32_t>(kind), &run_engine_op, done, user);
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_submit_port(mockingbird_engine_t* e, mockingbird_engine_op_t op,
                                                             mockingbird_post_cb post, int64_t port) {
    const int kind = engine_op(op);
    if (e == nullptr || kind < 0) return MOCKINGBIRD_ERR_INVALID_ARG;
    return submit_port(e->lifecycle, e, static_cast<uint32_t>(kind), &run_engine_op, post, port);
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_submit(mockingbird_player_t* p, mockingbird_player_op_t op,
                                                               mockingbird_done_cb done, void* user) {
    const int kind = player_op(op);
    if (p == nullptr || kind < 0) return MOCKINGBIRD_ERR_INVALID_ARG;
    return submit(p->lifecycle, p, static_cast<uint32_t>(kind), &run_player_op, done, user);
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_submit_port(mockingbird_player_t* p,
                                                                    mockingbird_player_op_t op,
                                                                    mockingbird_post_cb post, int64_t port) {
    const int kind = player_op(op);
    if (p == nullptr || kind < 0) return MOCKINGBIRD_ERR_INVALID_ARG;
    return submit_port(p->lifecycle, p, static_cast<uint32_t>(kind), &run_player_op, post, port);
}

}
