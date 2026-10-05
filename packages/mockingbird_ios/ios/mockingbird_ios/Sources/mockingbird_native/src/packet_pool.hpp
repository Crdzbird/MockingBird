// Pool of fixed-size payload slots behind the worker-to-host packet queue.
// Drained packets borrow a slot until the host releases it; a cookie made of
// slot index and checkout generation rejects stale or double releases.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace mockingbird {

class PacketPool {
public:
    struct Slot {
        std::unique_ptr<uint8_t[]> buf;
        uint32_t                   size          = 0;
        uint64_t                   segment_id    = 0;
        uint64_t                   pts_us        = 0;
        uint64_t                   capture_ts_us = 0;
        uint32_t                   flags         = 0;
        mockingbird_codec_t              codec         = MOCKINGBIRD_CODEC_PCM_S16LE;
        uint32_t                   index         = 0;
        uint32_t                   generation    = 0;
        bool                       in_flight     = false;
    };

    PacketPool(size_t pool_capacity, size_t queue_capacity, size_t max_payload_bytes);

    PacketPool(size_t capacity, size_t max_payload_bytes)
        : PacketPool(capacity, capacity, max_payload_bytes) {}

    PacketPool(const PacketPool&)            = delete;
    PacketPool& operator=(const PacketPool&) = delete;

    Slot* acquire();
    bool  commit(Slot* slot);
    void  release_unused(Slot* slot);

    bool           drain(mockingbird_packet_t* out);
    mockingbird_status_t release(const mockingbird_packet_t* pkt);

    size_t ready_size() const;
    size_t pool_capacity() const noexcept { return slots_.size(); }
    size_t max_payload_bytes() const noexcept { return max_payload_bytes_; }

private:
    void drop_oldest_locked();

    std::vector<std::unique_ptr<Slot>> slots_;
    size_t                             max_payload_bytes_;
    size_t                             ready_cap_;

    mutable std::mutex mu_;
    std::vector<Slot*> free_list_;
    std::deque<Slot*>  ready_;
};

} // namespace mockingbird
