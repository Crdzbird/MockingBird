// tests/test_packet_pool.cpp

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "packet_pool.hpp"

#include <cstdint>
#include <cstring>

using mockingbird::PacketPool;

namespace {
void fill_slot(PacketPool::Slot* s,
               uint8_t           pattern,
               uint64_t          segment_id,
               uint32_t          flags,
               uint32_t          size) {
    REQUIRE(s != nullptr);
    s->segment_id    = segment_id;
    s->pts_us        = 0;
    s->capture_ts_us = 0;
    s->flags         = flags;
    s->codec         = MOCKINGBIRD_CODEC_PCM_S16LE;
    s->size          = size;
    std::memset(s->buf.get(), pattern, size);
}

}

TEST_CASE("PacketPool initial state has all slots free") {
    PacketPool p(/*capacity=*/4, /*max_payload_bytes=*/64);
    CHECK(p.pool_capacity() == 4);
    CHECK(p.max_payload_bytes() == 64);
    CHECK(p.ready_size() == 0);
}

TEST_CASE("PacketPool acquire / commit / drain / release round trip") {
    PacketPool p(2, 32);

    auto* s = p.acquire();
    REQUIRE(s != nullptr);
    fill_slot(s, 0xAB, /*seg*/ 7, MOCKINGBIRD_PKT_FLAG_KEYFRAME, 32);
    CHECK_FALSE(p.commit(s));
    CHECK(p.ready_size() == 1);

    mockingbird_packet_t pkt{};
    REQUIRE(p.drain(&pkt));
    CHECK(pkt.segment_id == 7);
    CHECK(pkt.size       == 32);
    CHECK(pkt.codec      == MOCKINGBIRD_CODEC_PCM_S16LE);
    CHECK((pkt.flags & MOCKINGBIRD_PKT_FLAG_KEYFRAME) != 0);
    REQUIRE(pkt.data != nullptr);
    CHECK(pkt.data[0]    == 0xAB);
    CHECK(pkt.data[31]   == 0xAB);

    CHECK(p.release(&pkt) == MOCKINGBIRD_OK);
    CHECK(p.ready_size() == 0);

    auto* s2 = p.acquire();
    CHECK(s2 != nullptr);
}

TEST_CASE("PacketPool rejects stale-copy release after slot reuse (ABA guard)") {
    PacketPool p(/*pool_capacity=*/1, /*queue_capacity=*/1, /*max_payload_bytes=*/32);

    auto* s1 = p.acquire();
    fill_slot(s1, 0xAA, /*seg*/ 1, 0, 16);
    CHECK_FALSE(p.commit(s1));
    mockingbird_packet_t pkt1{};
    REQUIRE(p.drain(&pkt1));
    const mockingbird_packet_t stale = pkt1;
    CHECK(p.release(&pkt1) == MOCKINGBIRD_OK);

    auto* s2 = p.acquire();
    fill_slot(s2, 0xBB, /*seg*/ 2, 0, 16);
    CHECK_FALSE(p.commit(s2));
    mockingbird_packet_t pkt2{};
    REQUIRE(p.drain(&pkt2));

    CHECK(stale.data == pkt2.data);
    CHECK(p.release(&stale) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(p.release(&pkt2) == MOCKINGBIRD_OK);
}

TEST_CASE("PacketPool commit drops OLDEST when ready queue is at capacity") {
    PacketPool p(/*pool_capacity=*/3, /*queue_capacity=*/2, /*max_payload_bytes=*/16);

    auto* a = p.acquire();  fill_slot(a, 0x11, 1, 0, 16);  CHECK_FALSE(p.commit(a));
    auto* b = p.acquire();  fill_slot(b, 0x22, 2, 0, 16);  CHECK_FALSE(p.commit(b));
    CHECK(p.ready_size() == 2);

    auto* c = p.acquire();
    REQUIRE(c != nullptr);
    fill_slot(c, 0x33, 3, 0, 16);
    CHECK(p.commit(c));
    CHECK(p.ready_size() == 2);

    mockingbird_packet_t pkt2{};
    REQUIRE(p.drain(&pkt2));
    CHECK(pkt2.segment_id == 2);
    CHECK(p.release(&pkt2) == MOCKINGBIRD_OK);

    mockingbird_packet_t pkt3{};
    REQUIRE(p.drain(&pkt3));
    CHECK(pkt3.segment_id == 3);
    CHECK(p.release(&pkt3) == MOCKINGBIRD_OK);

    CHECK(p.ready_size() == 0);
}

TEST_CASE("PacketPool exhausts when pool_capacity equals queue_capacity and queue is full") {
    PacketPool p(/*capacity=*/2, /*max_payload_bytes=*/16);

    auto* a = p.acquire();  fill_slot(a, 0x11, 1, 0, 16);  CHECK_FALSE(p.commit(a));
    auto* b = p.acquire();  fill_slot(b, 0x22, 2, 0, 16);  CHECK_FALSE(p.commit(b));
    CHECK(p.acquire() == nullptr);
    CHECK(p.ready_size() == 2);
}

TEST_CASE("PacketPool::release rejects foreign packets") {
    PacketPool p(2, 16);
    mockingbird_packet_t fake{};
    fake._reserved = 999;
    fake.data      = reinterpret_cast<const uint8_t*>(0x1);
    CHECK(p.release(&fake) == MOCKINGBIRD_ERR_INVALID_ARG);

    auto* s = p.acquire();
    fill_slot(s, 0xCD, 5, 0, 4);
    p.commit(s);
    mockingbird_packet_t pkt{};
    REQUIRE(p.drain(&pkt));
    pkt.data = reinterpret_cast<const uint8_t*>(0xDEADBEEF);
    CHECK(p.release(&pkt) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("PacketPool::release(nullptr) returns INVALID_ARG, never crashes") {
    PacketPool p(1, 1);
    CHECK(p.release(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("PacketPool::drain on empty returns false") {
    PacketPool p(2, 8);
    mockingbird_packet_t pkt{};
    CHECK_FALSE(p.drain(&pkt));
}
