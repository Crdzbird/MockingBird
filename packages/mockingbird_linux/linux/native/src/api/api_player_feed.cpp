// mockingbird_player_push, _flush and _read: the producer and, for HOST_PCM,
// the consumer side of the player's slot queue.
#include "mockingbird/mockingbird.h"
#include "config_validation.hpp"
#include "play/player.hpp"

#include <cstring>

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_player_push(mockingbird_player_t* p, mockingbird_codec_t codec,
                                                             const uint8_t* data, uint32_t size,
                                                             uint64_t pts_us, uint32_t flags) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    mockingbird_status_t status = MOCKINGBIRD_OK;
    const int kind = mockingbird::enum_as_int(codec);
    const uint32_t frames = mockingbird::packet_frames_for(p->cfg, kind, data, size, status);
    if (status != MOCKINGBIRD_OK) return status;
    mockingbird::PacketSlot* slot = p->queue.acquire();
    if (slot == nullptr) {
        p->shared.count(p->shared.dropped);
        return MOCKINGBIRD_OK;
    }
    slot->pts_us     = pts_us;
    slot->generation = p->shared.generation.load(std::memory_order_acquire);
    slot->flags      = flags;
    slot->size       = size;
    slot->frames     = frames;
    slot->codec      = static_cast<mockingbird_codec_t>(kind);
    std::memcpy(slot->bytes, data, size);
    p->queue.submit(slot);
    p->shared.count(p->shared.received);
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_flush(mockingbird_player_t* p) {
    if (p == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    p->shared.generation.fetch_add(1, std::memory_order_acq_rel);
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_player_read(mockingbird_player_t* p, int16_t* out,
                                                             uint32_t frames) {
    if (p == nullptr || (out == nullptr && frames != 0)) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (p->cfg.backend != MOCKINGBIRD_BACKEND_HOST_PCM) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (frames > p->cfg.sample_rate_hz) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (!p->shared.started.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_NOT_RUNNING;
    if (frames == 0) return MOCKINGBIRD_OK;
    if (p->shared.paused.load(std::memory_order_acquire)) {
        std::memset(out, 0, size_t{frames} * p->cfg.channels * sizeof(int16_t));
        return MOCKINGBIRD_OK;
    }
    p->renderer.render(out, frames);
    return MOCKINGBIRD_OK;
}

}
