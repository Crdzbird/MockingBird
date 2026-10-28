// tests/test_preroll_ring.cpp

#include "doctest.h"
#include "preroll_ring.hpp"

#include <array>
#include <cstdint>
#include <vector>

using mockingbird::PrerollRing;

TEST_CASE("PrerollRing init(0, n) keeps push as a no-op") {
    PrerollRing r;
    r.init(/*capacity=*/0, /*frame_samples=*/8);
    std::array<int16_t, 8> f{1,2,3,4,5,6,7,8};
    r.push(f.data(), 1000);
    CHECK(r.empty());
    CHECK(r.size() == 0);
    CHECK(r.capacity() == 0);
}

TEST_CASE("PrerollRing fills, then overwrites the oldest") {
    constexpr size_t kCap   = 3;
    constexpr size_t kFrame = 4;
    PrerollRing r;
    r.init(kCap, kFrame);
    CHECK(r.capacity() == kCap);

    auto push = [&](int16_t tag, uint64_t ts) {
        std::array<int16_t, kFrame> f{tag, tag, tag, tag};
        r.push(f.data(), ts);
    };

    push(1, 100); CHECK(r.size() == 1);
    push(2, 200); CHECK(r.size() == 2);
    push(3, 300); CHECK(r.size() == 3);
    CHECK(r.oldest_ts_us() == 100);

    push(4, 400);
    CHECK(r.size() == 3);
    CHECK(r.oldest_ts_us() == 200);

    std::vector<int16_t> tags;
    std::vector<uint64_t> tss;
    r.for_each_oldest_first([&](const PrerollRing::Frame& f) {
        tags.push_back(f.samples[0]);
        tss.push_back(f.capture_ts_us);
    });
    CHECK(tags == std::vector<int16_t>{2, 3, 4});
    CHECK(tss  == std::vector<uint64_t>{200, 300, 400});

    r.clear();
    CHECK(r.empty());
    CHECK(r.oldest_ts_us() == 0);
}

TEST_CASE("PrerollRing iteration order survives multiple wraps") {
    PrerollRing r;
    r.init(/*capacity=*/3, /*frame_samples=*/1);
    for (int16_t i = 0; i < 10; ++i) {
        std::array<int16_t, 1> v{i};
        r.push(v.data(), static_cast<uint64_t>(i) * 10);
    }
    std::vector<int16_t> seen;
    r.for_each_oldest_first([&](const PrerollRing::Frame& f) {
        seen.push_back(f.samples[0]);
    });
    CHECK(seen == std::vector<int16_t>{7, 8, 9});
}
