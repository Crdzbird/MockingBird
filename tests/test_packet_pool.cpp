// tests/test_packet_pool.cpp

#include "doctest.h"
#include "auris/auris.h"
#include "packet_pool.hpp"

#include <cstdint>
#include <cstring>

using auris::PacketPool;

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
    s->codec         = AURIS_CODEC_PCM_S16LE;
    s->size          = size;
    std::memset(s->buf.get(), pattern, size);
}

} // namespace

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
    fill_slot(s, 0xAB, /*seg*/ 7, AURIS_PKT_FLAG_KEYFRAME, 32);
    CHECK_FALSE(p.commit(s));   // commit clean, no overrun
    CHECK(p.ready_size() == 1);

    auris_packet_t pkt{};
    REQUIRE(p.drain(&pkt));
    CHECK(pkt.segment_id == 7);
    CHECK(pkt.size       == 32);
    CHECK(pkt.codec      == AURIS_CODEC_PCM_S16LE);
    CHECK((pkt.flags & AURIS_PKT_FLAG_KEYFRAME) != 0);
    REQUIRE(pkt.data != nullptr);
    CHECK(pkt.data[0]    == 0xAB);
    CHECK(pkt.data[31]   == 0xAB);

    CHECK(p.release(&pkt) == AURIS_OK);
    CHECK(p.ready_size() == 0);

    // Slot should be acquirable again.
    auto* s2 = p.acquire();
    CHECK(s2 != nullptr);
}

TEST_CASE("PacketPool commit drops OLDEST when ready queue is at capacity") {
    // pool_capacity > queue_capacity so the worker can still acquire a slot
    // after the ready queue is full. The extra slot is the "borrow budget"
    // for the host; in this test we exercise the worker path instead.
    PacketPool p(/*pool_capacity=*/3, /*queue_capacity=*/2, /*max_payload_bytes=*/16);

    auto* a = p.acquire();  fill_slot(a, 0x11, 1, 0, 16);  CHECK_FALSE(p.commit(a));
    auto* b = p.acquire();  fill_slot(b, 0x22, 2, 0, 16);  CHECK_FALSE(p.commit(b));
    CHECK(p.ready_size() == 2);   // queue at cap

    // Third commit must drop the oldest (segment 1) before pushing segment 3.
    auto* c = p.acquire();
    REQUIRE(c != nullptr);        // pool still has a free slot
    fill_slot(c, 0x33, 3, 0, 16);
    CHECK(p.commit(c));            // returns true: overran
    CHECK(p.ready_size() == 2);

    // FIFO order should now be {2, 3} (segment 1 was dropped on the third commit).
    auris_packet_t pkt2{};
    REQUIRE(p.drain(&pkt2));
    CHECK(pkt2.segment_id == 2);
    CHECK(p.release(&pkt2) == AURIS_OK);

    auris_packet_t pkt3{};
    REQUIRE(p.drain(&pkt3));
    CHECK(pkt3.segment_id == 3);
    CHECK(p.release(&pkt3) == AURIS_OK);

    CHECK(p.ready_size() == 0);
}

TEST_CASE("PacketPool exhausts when pool_capacity equals queue_capacity and queue is full") {
    PacketPool p(/*capacity=*/2, /*max_payload_bytes=*/16);

    auto* a = p.acquire();  fill_slot(a, 0x11, 1, 0, 16);  CHECK_FALSE(p.commit(a));
    auto* b = p.acquire();  fill_slot(b, 0x22, 2, 0, 16);  CHECK_FALSE(p.commit(b));
    // Ready queue is full and pool has no free slots: worker would have to
    // wait for the host to release. This is the engine's default shape.
    CHECK(p.acquire() == nullptr);
    CHECK(p.ready_size() == 2);
}

TEST_CASE("PacketPool::release rejects foreign packets") {
    PacketPool p(2, 16);
    auris_packet_t fake{};
    fake._reserved = 999;          // out-of-range slot index
    fake.data      = reinterpret_cast<const uint8_t*>(0x1);
    CHECK(p.release(&fake) == AURIS_ERR_INVALID_ARG);

    // Valid index but wrong data pointer.
    auto* s = p.acquire();
    fill_slot(s, 0xCD, 5, 0, 4);
    p.commit(s);
    auris_packet_t pkt{};
    REQUIRE(p.drain(&pkt));
    pkt.data = reinterpret_cast<const uint8_t*>(0xDEADBEEF);
    CHECK(p.release(&pkt) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("PacketPool::release(nullptr) returns INVALID_ARG, never crashes") {
    PacketPool p(1, 1);
    CHECK(p.release(nullptr) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("PacketPool::drain on empty returns false") {
    PacketPool p(2, 8);
    auris_packet_t pkt{};
    CHECK_FALSE(p.drain(&pkt));
}
