// One mutex guards the free list and the ready queue; at a 50 Hz packet
// rate it is never contended.
#include "packet_pool.hpp"

namespace mockingbird {

PacketPool::PacketPool(size_t pool_capacity, size_t queue_capacity, size_t max_payload_bytes)
    : max_payload_bytes_(max_payload_bytes),
      ready_cap_(queue_capacity == 0 ? 1 : queue_capacity) {
    const size_t cap = (pool_capacity == 0) ? 1 : pool_capacity;
    slots_.reserve(cap);
    free_list_.reserve(cap);
    for (size_t i = 0; i < cap; ++i) {
        auto s   = std::make_unique<Slot>();
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

bool PacketPool::drain(mockingbird_packet_t* out) {
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
    ++s->generation;
    out->_reserved = ((s->generation & 0xFFFFu) << 16) | (s->index & 0xFFFFu);
    s->in_flight   = true;
    return true;
}

mockingbird_status_t PacketPool::release(const mockingbird_packet_t* pkt) {
    if (pkt == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    const uint32_t cookie = pkt->_reserved;
    const uint32_t idx    = cookie & 0xFFFFu;
    const uint32_t gen    = (cookie >> 16) & 0xFFFFu;
    std::lock_guard lk(mu_);
    if (idx >= slots_.size()) return MOCKINGBIRD_ERR_INVALID_ARG;
    Slot* s = slots_[idx].get();
    if (pkt->data != s->buf.get()) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (!s->in_flight) return MOCKINGBIRD_ERR_INVALID_ARG;
    if ((s->generation & 0xFFFFu) != gen) return MOCKINGBIRD_ERR_INVALID_ARG;
    s->in_flight = false;
    s->size      = 0;
    free_list_.push_back(s);
    return MOCKINGBIRD_OK;
}

size_t PacketPool::ready_size() const {
    std::lock_guard lk(mu_);
    return ready_.size();
}

} // namespace mockingbird
