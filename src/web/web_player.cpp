// Player entry points for the WebAssembly shim. The web always plays through
// HOST_PCM, pulled by Web Audio; pts crosses as a double because a 64-bit
// integer would reach JavaScript as a BigInt, and stats are read by field.
#ifdef __EMSCRIPTEN__

#include "mockingbird/mockingbird.h"

#include <emscripten/emscripten.h>

#include <cstdint>

extern "C" {
EMSCRIPTEN_KEEPALIVE mockingbird_player_t* mockingbird_web_player_create(uint32_t rate, uint32_t channels,
                                                                        uint32_t target_ms, uint32_t max_ms,
                                                                        uint32_t conceal_ms, uint32_t queue,
                                                                        int32_t* out_status) {
    mockingbird_player_config_t cfg{};
    mockingbird_player_config_defaults(&cfg);
    cfg.backend         = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.sample_rate_hz  = rate;
    cfg.channels        = static_cast<uint16_t>(channels > 0xFFFFu ? 0u : channels);
    cfg.target_delay_ms = target_ms;
    cfg.max_delay_ms    = max_ms;
    cfg.conceal_max_ms  = conceal_ms;
    cfg.queue_packets   = queue;
    mockingbird_player_t* p = nullptr;
    const mockingbird_status_t s = mockingbird_player_create(&cfg, &p);
    if (out_status != nullptr) *out_status = static_cast<int32_t>(s);
    return p;
}

EMSCRIPTEN_KEEPALIVE int mockingbird_web_player_push(mockingbird_player_t* p, int codec, const uint8_t* data,
                                                     uint32_t size, double pts_us, uint32_t flags) {
    if (codec < 0 || codec > MOCKINGBIRD_CODEC_OGG_OPUS || !(pts_us >= 0.0)) return MOCKINGBIRD_ERR_INVALID_ARG;
    return static_cast<int>(mockingbird_player_push(p, static_cast<mockingbird_codec_t>(codec), data, size,
                                                    static_cast<uint64_t>(pts_us), flags));
}

EMSCRIPTEN_KEEPALIVE double mockingbird_web_player_stat(mockingbird_player_t* p, int field) {
    mockingbird_player_stats_t s{};
    if (mockingbird_player_get_stats(p, &s) != MOCKINGBIRD_OK) return -1.0;
    switch (field) {
        case 0:  return static_cast<double>(s.packets_received);
        case 1:  return static_cast<double>(s.packets_played);
        case 2:  return static_cast<double>(s.packets_concealed);
        case 3:  return static_cast<double>(s.packets_dropped);
        case 4:  return static_cast<double>(s.packets_late);
        case 5:  return static_cast<double>(s.underruns);
        case 6:  return static_cast<double>(s.frames_rendered);
        case 7:  return static_cast<double>(s.buffered_packets);
        case 8:  return static_cast<double>(s.buffered_ms);
        case 9:  return static_cast<double>(s.state);
        case 10: return static_cast<double>(s.output_restarts);
        default: return 0.0;
    }
}

}

#endif
