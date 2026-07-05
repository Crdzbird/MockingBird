// src/packet_pool.hpp
//
// Pool of fixed-size payload buffers backing the worker-to-host packet queue.
//
// // ZERO-COPY  Slot.buf is the storage that backs auris_packet.data when
// drained. Bytes are owned by the slot; auris_packet borrows a const pointer.
// Lifetime ends when the host calls auris_release and the slot returns to the
// free list.
//
// Concurrency: producer is T2 (worker), consumer is T3 (host, single-threaded
// per the ABI contract). One std::mutex protects both the free list and the
// ready deque; at the packet rate this engine emits (≈50 Hz for 20 ms frames)
// contention is a non-issue. If profiling later shows pressure, this becomes
// a lock-free SPSC queue + atomic free stack without changing the public
// surface.
//
// Drop-oldest on full (§4): commit() pops the oldest ready packet back into
// the free list before queuing the new one and returns true so the worker can
// emit AURIS_EVT_OVERRUN.

#pragma once

#include "auris/auris.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace auris {

class PacketPool {
public:
    struct Slot {
        std::unique_ptr<uint8_t[]> buf;            // capacity == max_payload_bytes
        uint32_t                   size          = 0;
        uint64_t                   segment_id    = 0;
        uint64_t                   pts_us        = 0;
        uint64_t                   capture_ts_us = 0;
        uint32_t                   flags         = 0;
        auris_codec_t              codec         = AURIS_CODEC_PCM_S16LE;
        uint32_t                   index         = 0;  // self-index, set at init
        uint32_t                   generation    = 0;  // bumped each drain (ABA guard)
        bool                       in_flight     = false; // drained, awaiting release
    };

    // pool_capacity is the total slot count; queue_capacity is the bounded
    // ready-queue depth. pool_capacity >= queue_capacity always; the
    // difference is the host's borrow budget (how many drained packets the
    // host can hold without starving the worker).
    PacketPool(size_t pool_capacity, size_t queue_capacity, size_t max_payload_bytes);

    // Convenience overload: pool == queue (no borrow budget). Kept for
    // call sites that have no opinion on the split.
    PacketPool(size_t capacity, size_t max_payload_bytes)
        : PacketPool(capacity, capacity, max_payload_bytes) {}

    PacketPool(const PacketPool&)            = delete;
    PacketPool& operator=(const PacketPool&) = delete;

    // Worker side. Returns nullptr if the pool is exhausted (host is holding
    // borrowed packets and has not released them). Treat null as a packets-
    // dropped signal.
    Slot* acquire();

    // Worker side. Enqueues a filled slot in the ready queue. If the queue is
    // at capacity, drops the OLDEST ready slot back into the free list and
    // returns true. Returns false on a clean commit.
    bool commit(Slot* slot);

    // Worker side. Return a slot to the free list without queueing it.
    // Used when the encoder rejected the frame (DTX / error) so the worker
    // does not have to commit a zero-byte packet.
    void release_unused(Slot* slot);

    // Host side. Pops the next ready slot and populates *out with the borrowed
    // view. Returns false if nothing is ready.
    bool drain(auris_packet_t* out);

    // Host side. Returns a packet to the pool. Validates the cookie + pointer;
    // returns AURIS_ERR_INVALID_ARG on mismatch.
    auris_status_t release(const auris_packet_t* pkt);

    size_t ready_size() const;
    size_t pool_capacity() const noexcept { return slots_.size(); }
    size_t max_payload_bytes() const noexcept { return max_payload_bytes_; }

private:
    void drop_oldest_locked();

    std::vector<std::unique_ptr<Slot>> slots_;
    size_t                             max_payload_bytes_;
    size_t                             ready_cap_;

    mutable std::mutex                 mu_;
    std::vector<Slot*>                 free_list_;   // LIFO is fine; warmer cache.
    std::deque<Slot*>                  ready_;
};

} // namespace auris
