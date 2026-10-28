// A pool with no free slot, or a full ready queue, drops a packet and
// reports it as an overrun; an encoder returning no bytes recycles the slot.
#include "worker/packet_emitter.hpp"

#include "engine.hpp"
#include "engine_events.hpp"

namespace mockingbird {
void PacketEmitter::init(mockingbird_engine* e, uint32_t frame_samples) {
    e_             = e;
    frame_samples_ = frame_samples;
}

void PacketEmitter::overrun(uint64_t now_ts) {
    e_->packets_dropped.fetch_add(1, std::memory_order_relaxed);
    emit_event(e_, make_event(MOCKINGBIRD_EVT_OVERRUN, 0, now_ts));
}

void PacketEmitter::emit(const int16_t* samples, uint64_t now_ts, uint32_t flags,
                         uint64_t segment_id, uint64_t segment_start_us,
                         uint32_t& segment_pkt_index) {
    auto* slot = e_->packets.acquire();
    if (slot == nullptr) {
        overrun(now_ts);
        return;
    }
    const int n = e_->encoder->encode(samples, frame_samples_, slot->buf.get(),
                                      static_cast<uint32_t>(e_->packets.max_payload_bytes()));
    if (n <= 0) {
        slot->size = 0;
        e_->packets.release_unused(slot);
        if (n < 0) e_->packets_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    slot->codec         = e_->encoder->codec();
    slot->segment_id    = segment_id;
    slot->capture_ts_us = now_ts;
    slot->pts_us        = (now_ts >= segment_start_us) ? (now_ts - segment_start_us) : 0;
    slot->flags         = segment_pkt_index == 0 ? (flags | MOCKINGBIRD_PKT_FLAG_KEYFRAME) : flags;
    slot->size          = static_cast<uint32_t>(n);

    const bool overran = e_->packets.commit(slot);
    e_->packets_emitted.fetch_add(1, std::memory_order_relaxed);
    ++segment_pkt_index;
    if (overran) overrun(now_ts);
}

}
