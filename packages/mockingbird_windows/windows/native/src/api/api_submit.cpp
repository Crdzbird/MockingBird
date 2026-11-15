// mockingbird_submit and mockingbird_player_submit: the blocking lifecycle
// calls run on the engine's or the player's own lifecycle thread, in order,
// with the result reported through the caller's done callback.
#include "mockingbird/mockingbird.h"
#include "config_validation.hpp"
#include "engine.hpp"
#include "play/player.hpp"

namespace {
mockingbird_status_t run_engine_op(mockingbird_engine_t* e, uint32_t op) {
    if (op == MOCKINGBIRD_ENGINE_OP_START) return mockingbird_start(e);
    return mockingbird_stop(e);
}

mockingbird_status_t run_player_op(mockingbird_player_t* p, uint32_t op) {
    switch (op) {
        case MOCKINGBIRD_PLAYER_OP_START:
            return mockingbird_player_start(p);
        case MOCKINGBIRD_PLAYER_OP_PAUSE:
            return mockingbird_player_pause(p);
        case MOCKINGBIRD_PLAYER_OP_RESUME:
            return mockingbird_player_resume(p);
        default:
            return mockingbird_player_stop(p);
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

}

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_submit(mockingbird_engine_t* e, mockingbird_engine_op_t op,
                                                        mockingbird_done_cb done, void* user) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    const int kind = mockingbird::enum_as_int(op);
    if (kind < MOCKINGBIRD_ENGINE_OP_START || kind > MOCKINGBIRD_ENGINE_OP_STOP) return MOCKINGBIRD_ERR_INVALID_ARG;
    return submit(e->lifecycle, e, static_cast<uint32_t>(kind), &run_engine_op, done, user);
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_submit(mockingbird_player_t* p, mockingbird_player_op_t op,
                                                               mockingbird_done_cb done, void* user) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    const int kind = mockingbird::enum_as_int(op);
    if (kind < MOCKINGBIRD_PLAYER_OP_START || kind > MOCKINGBIRD_PLAYER_OP_STOP) return MOCKINGBIRD_ERR_INVALID_ARG;
    return submit(p->lifecycle, p, static_cast<uint32_t>(kind), &run_player_op, done, user);
}

}
