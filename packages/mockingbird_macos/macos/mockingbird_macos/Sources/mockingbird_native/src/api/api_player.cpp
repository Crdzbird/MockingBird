// mockingbird_player_config_defaults, _create and _destroy.
#include "mockingbird/mockingbird.h"
#include "play/player.hpp"

#include <new>

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_player_config_defaults(mockingbird_player_config_t* out_cfg) {
    if (out_cfg == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    *out_cfg = mockingbird_player_config_t{};
    out_cfg->backend         = MOCKINGBIRD_BACKEND_AUTO;
    out_cfg->sample_rate_hz  = 16000;
    out_cfg->channels        = 1;
    out_cfg->target_delay_ms = 60;
    out_cfg->max_delay_ms    = 400;
    out_cfg->conceal_max_ms  = 100;
    out_cfg->queue_packets   = 128;
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_create(const mockingbird_player_config_t* cfg,
                                                               mockingbird_player_t** out_player) {
    if (out_player == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    *out_player = nullptr;
    if (cfg == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    const mockingbird_status_t valid = mockingbird::validate_player_config(*cfg);
    if (valid != MOCKINGBIRD_OK) return valid;
    try {
        auto p = std::make_unique<mockingbird_player>(*cfg);
        const mockingbird_status_t s = p->renderer.init(*cfg, &p->queue, &p->shared);
        if (s != MOCKINGBIRD_OK) return s;
        *out_player = p.release();
        return MOCKINGBIRD_OK;
    } catch (const std::bad_alloc&) {
        return MOCKINGBIRD_ERR_NO_MEMORY;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API void mockingbird_player_destroy(mockingbird_player_t* player) {
    if (player == nullptr) return;
    mockingbird_player_stop(player);
    player->output.reset();
    delete player;
}

}
