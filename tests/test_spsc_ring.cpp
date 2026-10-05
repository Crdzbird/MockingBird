// tests/test_spsc_ring.cpp
//
// SPSC ring is the PCM hot path; correctness here gates everything else.

#include "doctest.h"
#include "spsc_ring.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using mockingbird::SpscRing;

TEST_CASE("round_up_pow2 covers boundary cases") {
    CHECK(mockingbird::round_up_pow2(0)    == 1);
    CHECK(mockingbird::round_up_pow2(1)    == 1);
    CHECK(mockingbird::round_up_pow2(2)    == 2);
    CHECK(mockingbird::round_up_pow2(3)    == 4);
    CHECK(mockingbird::round_up_pow2(5)    == 8);
    CHECK(mockingbird::round_up_pow2(1000) == 1024);
    CHECK(mockingbird::round_up_pow2(1024) == 1024);
    CHECK(mockingbird::round_up_pow2(1025) == 2048);
}

TEST_CASE("SpscRing rounds capacity up to power of two") {
    SpscRing<int16_t> r(1000);
    CHECK(r.capacity() == 1024);
    CHECK(r.size_relaxed() == 0);
}

TEST_CASE("SpscRing single-shot push/pop preserves bytes") {
    SpscRing<int16_t> r(16);
    std::array<int16_t, 8> in{1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(r.push(in.data(), in.size()) == in.size());
    CHECK(r.size_relaxed() == in.size());

    std::array<int16_t, 8> out{};
    CHECK(r.pop(out.data(), out.size()) == out.size());
    CHECK(r.size_relaxed() == 0);
    for (size_t i = 0; i < in.size(); ++i) CHECK(in[i] == out[i]);
}

TEST_CASE("SpscRing drops NEWEST on full, never partial-overwrites") {
    SpscRing<int16_t> r(8);
    std::array<int16_t, 8> first{1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(r.push(first.data(), 8) == 8);

    std::array<int16_t, 4> extra{9, 9, 9, 9};
    CHECK(r.push(extra.data(), 4) == 0);          // ring full, nothing accepted

    std::array<int16_t, 8> out{};
    CHECK(r.pop(out.data(), 8) == 8);
    for (size_t i = 0; i < 8; ++i) CHECK(out[i] == first[i]);
}

TEST_CASE("SpscRing wraparound preserves byte order") {
    SpscRing<int16_t> r(8);
    std::array<int16_t, 5> a{1, 2, 3, 4, 5};
    std::array<int16_t, 5> b{10, 20, 30, 40, 50};

    CHECK(r.push(a.data(), a.size()) == 5);
    std::array<int16_t, 3> drain{};
    CHECK(r.pop(drain.data(), 3) == 3);           // tail advances past mask boundary
    CHECK(r.push(b.data(), b.size()) == 5);       // writes wrap past end of buffer

    std::array<int16_t, 7> out{};
    CHECK(r.pop(out.data(), out.size()) == 7);
    const int16_t want[7] = {4, 5, 10, 20, 30, 40, 50};
    for (size_t i = 0; i < 7; ++i) CHECK(out[i] == want[i]);
}

TEST_CASE("SpscRing pop on empty returns zero, never blocks") {
    SpscRing<int16_t> r(16);
    std::array<int16_t, 4> dst{};
    CHECK(r.pop(dst.data(), dst.size()) == 0);
}

TEST_CASE("SpscRing concurrent producer/consumer preserves every byte") {
    // Stress test: a producer pushes a known sequence; a consumer pops it
    // back. Both run on dedicated threads. Verifies the memory ordering
    // pairing (release on store, acquire on load) actually carries data.
    constexpr size_t kCapacity = 1024;
    constexpr size_t kTotal    = 200000;
    SpscRing<uint32_t> r(kCapacity);

    std::atomic<bool> ready{false};
    std::vector<uint32_t> consumed;
    consumed.reserve(kTotal);

    std::thread cons([&] {
        while (!ready.load(std::memory_order_acquire)) {}
        std::array<uint32_t, 64> tmp{};
        while (consumed.size() < kTotal) {
            const size_t n = r.pop(tmp.data(), tmp.size());
            for (size_t i = 0; i < n; ++i) consumed.push_back(tmp[i]);
        }
    });

    std::thread prod([&] {
        ready.store(true, std::memory_order_release);
        size_t pushed = 0;
        std::array<uint32_t, 64> tmp{};
        while (pushed < kTotal) {
            const size_t batch = std::min<size_t>(tmp.size(), kTotal - pushed);
            for (size_t i = 0; i < batch; ++i) tmp[i] = static_cast<uint32_t>(pushed + i);
            const size_t n = r.push(tmp.data(), batch);
            pushed += n;
            // If full, spin briefly; consumer will drain.
        }
    });

    prod.join();
    cons.join();

    REQUIRE(consumed.size() == kTotal);
    for (size_t i = 0; i < kTotal; ++i) {
        REQUIRE(consumed[i] == i);
    }
}
