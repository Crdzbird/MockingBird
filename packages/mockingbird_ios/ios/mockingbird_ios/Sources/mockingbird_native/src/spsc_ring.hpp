// Single-producer, single-consumer lock-free ring; capacity rounds up to a
// power of two and a full ring drops the newest samples.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace mockingbird {

inline constexpr size_t kCacheLine = 64;

inline size_t round_up_pow2(size_t v) noexcept {
    if (v < 2) return 1;
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    if constexpr (sizeof(size_t) > 4) v |= v >> 32;
    return v + 1;
}

template <typename T>
class SpscRing {
    static_assert(std::is_trivially_copyable_v<T>, "SpscRing requires trivially copyable T");

public:
    explicit SpscRing(size_t requested_capacity) {
        capacity_ = round_up_pow2(requested_capacity == 0 ? 1 : requested_capacity);
        mask_     = capacity_ - 1;
        buf_      = std::make_unique<T[]>(capacity_);
    }

    SpscRing(const SpscRing&)            = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    size_t capacity() const noexcept { return capacity_; }

    size_t push(const T* src, size_t n) noexcept {
        const size_t head     = head_.load(std::memory_order_relaxed);
        const size_t tail     = tail_.load(std::memory_order_acquire);
        const size_t room     = capacity_ - (head - tail);
        const size_t to_write = (n < room) ? n : room;
        if (to_write == 0) return 0;
        copy_in(head & mask_, src, to_write);
        head_.store(head + to_write, std::memory_order_release);
        return to_write;
    }

    size_t pop(T* dst, size_t n) noexcept {
        const size_t tail    = tail_.load(std::memory_order_relaxed);
        const size_t head    = head_.load(std::memory_order_acquire);
        const size_t avail   = head - tail;
        const size_t to_read = (n < avail) ? n : avail;
        if (to_read == 0) return 0;
        copy_out(tail & mask_, dst, to_read);
        tail_.store(tail + to_read, std::memory_order_release);
        return to_read;
    }

    size_t size_relaxed() const noexcept {
        return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
    }

private:
    void copy_in(size_t at, const T* src, size_t n) noexcept {
        const size_t first = capacity_ - at;
        if (n <= first) {
            std::memcpy(&buf_[at], src, n * sizeof(T));
            return;
        }
        std::memcpy(&buf_[at], src, first * sizeof(T));
        std::memcpy(&buf_[0], src + first, (n - first) * sizeof(T));
    }

    void copy_out(size_t at, T* dst, size_t n) const noexcept {
        const size_t first = capacity_ - at;
        if (n <= first) {
            std::memcpy(dst, &buf_[at], n * sizeof(T));
            return;
        }
        std::memcpy(dst, &buf_[at], first * sizeof(T));
        std::memcpy(dst + first, &buf_[0], (n - first) * sizeof(T));
    }

    std::unique_ptr<T[]> buf_;
    size_t               capacity_ = 0;
    size_t               mask_     = 0;

    alignas(kCacheLine) std::atomic<size_t> head_{0};
    alignas(kCacheLine) std::atomic<size_t> tail_{0};
};

} // namespace mockingbird
