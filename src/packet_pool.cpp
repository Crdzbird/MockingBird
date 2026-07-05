// src/packet_pool.cpp
#include "packet_pool.hpp"

namespace auris {

PacketPool::PacketPool(size_t pool_capacity,
                       size_t queue_capacity,
                       size_t max_payload_bytes)
    : max_payload_bytes_(max_payload_bytes),
      ready_cap_(queue_capacity == 0 ? 1 : queue_capacity) {
    const size_t cap = (pool_capacity == 0) ? 1 : pool_capacity;
    slots_.reserve(cap);
    free_list_.reserve(cap);
    for (size_t i = 0; i < cap; ++i) {
        auto s = std::make_unique<Slot>();
        s->buf   = std::make_unique<uint8_t[]>(max_payload_bytes_);
        s->index = static_cast<uint32_t>(i);
        free_list_.push_back(s.get());
        slots_.push_back(std::move(s));
    }
}

PacketPool::Slot* PacketPool::acquire() {
    std::lock_guard lk(mu_);
    if (free_list_.empty()) return nullptr;
    Slot* s = free_list_.back();
    free_list_.pop_back();
    return s;
}

void PacketPool::drop_oldest_locked() {
    if (ready_.empty()) return;
    Slot* victim = ready_.front();
    ready_.pop_front();
    victim->size = 0;
    free_list_.push_back(victim);
}

void PacketPool::release_unused(Slot* slot) {
    if (slot == nullptr) return;
    std::lock_guard lk(mu_);
    slot->size = 0;
    free_list_.push_back(slot);
}

bool PacketPool::commit(Slot* slot) {
    if (slot == nullptr) return false;
    std::lock_guard lk(mu_);
    bool overran = false;
    while (ready_.size() >= ready_cap_) {
        drop_oldest_locked();
        overran = true;
    }
    ready_.push_back(slot);
    return overran;
}

bool PacketPool::drain(auris_packet_t* out) {
    if (out == nullptr) return false;
    std::lock_guard lk(mu_);
    if (ready_.empty()) return false;
    Slot* s = ready_.front();
    ready_.pop_front();
    out->segment_id    = s->segment_id;
    out->pts_us        = s->pts_us;
    out->capture_ts_us = s->capture_ts_us;
    out->flags         = s->flags;
    out->codec         = s->codec;
    out->data          = s->buf.get();
    out->size          = s->size;
    // Cookie = generation:index (16 bits each). The generation is bumped on
    // every checkout, so a stale packet copy released after the slot was reused
    // is rejected even though its index and data pointer still match (ABA guard).
    ++s->generation;
    out->_reserved     = ((s->generation & 0xFFFFu) << 16) | (s->index & 0xFFFFu);
    s->in_flight       = true;   // checked out to the host until released
    return true;
}

auris_status_t PacketPool::release(const auris_packet_t* pkt) {
    if (pkt == nullptr) return AURIS_ERR_INVALID_ARG;
    const uint32_t cookie = pkt->_reserved;
    const uint32_t idx    = cookie & 0xFFFFu;
    const uint32_t gen    = (cookie >> 16) & 0xFFFFu;
    std::lock_guard lk(mu_);
    if (idx >= slots_.size()) return AURIS_ERR_INVALID_ARG;
    Slot* s = slots_[idx].get();
    if (pkt->data != s->buf.get()) return AURIS_ERR_INVALID_ARG;
    // Reject double-release / release of a slot that isn't currently checked
    // out. Without this, a second release would push the slot onto the free
    // list twice and acquire() would later hand the same buffer to two
    // packets — a use-after-free / aliasing corruption from host misuse.
    if (!s->in_flight) return AURIS_ERR_INVALID_ARG;
    // Reject a stale/forged cookie: a copy of a previously-drained packet whose
    // slot has since been re-checked-out has a matching index+pointer+in_flight
    // but a mismatched generation. Without this the stale release would free a
    // slot the CURRENT holder still borrows → use-after-free of its bytes.
    if ((s->generation & 0xFFFFu) != gen) return AURIS_ERR_INVALID_ARG;
    s->in_flight = false;
    s->size = 0;
    free_list_.push_back(s);
    return AURIS_OK;
}

size_t PacketPool::ready_size() const {
    std::lock_guard lk(mu_);
    return ready_.size();
}

} // namespace auris
