// src/spsc_ring.hpp
//
// Single-producer / single-consumer lock-free ring of trivially-copyable
// samples. Power-of-two capacity (rounded up at construction). Cacheline-
// separated head/tail to avoid false sharing on hot paths.
//
// // PERF-CRITICAL  push() runs on T1 (RT capture thread). It is alloc-free,
// branch-light, and never blocks.
//
// // CONTRACT  Single producer thread, single consumer thread. The producer
// owns head_; the consumer owns tail_. Concurrent access by multiple producers
// or multiple consumers is undefined.
//
// On full: producer drops NEWEST (returns count_written < n). Per §4 the PCM
// ring should not overflow in healthy operation. The frames_dropped_capture
// stat exists as a health signal.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace auris {

inline constexpr size_t kCacheLine = 64;

// Round up to the next power of two; minimum 1.
inline size_t round_up_pow2(size_t v) noexcept {
    if (v < 2) return 1;
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    if constexpr (sizeof(size_t) > 4) {
        v |= v >> 32;
    }
    return v + 1;
}

template <typename T>
class SpscRing {
    static_assert(std::is_trivially_copyable_v<T>,
                  "SpscRing requires trivially copyable T");

public:
    explicit SpscRing(size_t requested_capacity) {
        capacity_ = round_up_pow2(requested_capacity == 0 ? 1 : requested_capacity);
        mask_     = capacity_ - 1;
        buf_      = std::make_unique<T[]>(capacity_);
    }

    SpscRing(const SpscRing&)            = delete;
    SpscRing& operator=(const SpscRing&) = delete;
    SpscRing(SpscRing&&)                 = delete;
    SpscRing& operator=(SpscRing&&)      = delete;

    size_t capacity() const noexcept { return capacity_; }

    // Producer side. Returns the number of items accepted (0..n).
    // Drops NEWEST on full: caller detects drop via returned < n.
    size_t push(const T* src, size_t n) noexcept {
        const size_t head      = head_.load(std::memory_order_relaxed);
        const size_t tail      = tail_.load(std::memory_order_acquire);
        const size_t in_flight = head - tail;
        const size_t free_room = capacity_ - in_flight;
        const size_t to_write  = (n < free_room) ? n : free_room;
        if (to_write == 0) return 0;

        const size_t first_idx   = head & mask_;
        const size_t first_chunk = capacity_ - first_idx;
        if (to_write <= first_chunk) {
            std::memcpy(&buf_[first_idx], src, to_write * sizeof(T));
        } else {
            std::memcpy(&buf_[first_idx], src, first_chunk * sizeof(T));
            std::memcpy(&buf_[0],
                        src + first_chunk,
                        (to_write - first_chunk) * sizeof(T));
        }
        head_.store(head + to_write, std::memory_order_release);
        return to_write;
    }

    // Consumer side. Returns the number of items read (0..n).
    size_t pop(T* dst, size_t n) noexcept {
        const size_t tail     = tail_.load(std::memory_order_relaxed);
        const size_t head     = head_.load(std::memory_order_acquire);
        const size_t avail    = head - tail;
        const size_t to_read  = (n < avail) ? n : avail;
        if (to_read == 0) return 0;

        const size_t first_idx   = tail & mask_;
        const size_t first_chunk = capacity_ - first_idx;
        if (to_read <= first_chunk) {
            std::memcpy(dst, &buf_[first_idx], to_read * sizeof(T));
        } else {
            std::memcpy(dst, &buf_[first_idx], first_chunk * sizeof(T));
            std::memcpy(dst + first_chunk,
                        &buf_[0],
                        (to_read - first_chunk) * sizeof(T));
        }
        tail_.store(tail + to_read, std::memory_order_release);
        return to_read;
    }

    // Snapshot of in-flight count. Either side may call; result may be stale.
    size_t size_relaxed() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return head - tail;
    }

private:
    std::unique_ptr<T[]> buf_;
    size_t               capacity_ = 0;
    size_t               mask_     = 0;

    alignas(kCacheLine) std::atomic<size_t> head_{0};
    alignas(kCacheLine) std::atomic<size_t> tail_{0};
};

} // namespace auris
