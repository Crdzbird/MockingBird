// tests/test_ogg_opus.cpp
//
// Segment endings and the Ogg Opus container. Every closed segment ends on
// a packet flagged LAST, the length cap splits a long segment, the post-roll
// sets how much trailing audio a segment keeps, and an Ogg Opus segment
// concatenates into a valid stream: header pages first, CRCs that check,
// consecutive page numbers, granules in 48 kHz units and end-of-stream last.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <thread>
#include <vector>

namespace {
struct Packet {
    uint64_t             segment_id = 0;
    uint32_t             flags      = 0;
    mockingbird_codec_t  codec      = MOCKINGBIRD_CODEC_PCM_S16LE;
    std::vector<uint8_t> bytes;
};

struct Page {
    uint8_t              flags   = 0;
    int64_t              granule = 0;
    uint32_t             serial  = 0;
    uint32_t             seq     = 0;
    std::vector<uint8_t> body;
};

struct Run {
    std::vector<Packet>   packets;
    uint32_t              segment_begins = 0;
    uint32_t              segment_ends   = 0;
};

std::vector<int16_t> tone(double amp01) {
    std::vector<int16_t> v(320);
    for (uint32_t i = 0; i < 320; ++i) {
        const double t = static_cast<double>(i) / 16000.0;
        v[i] = static_cast<int16_t>(
            std::lround(amp01 * 32767.0 * std::sin(2.0 * 3.14159265358979323846 * 800.0 * t)));
    }
    return v;
}

mockingbird_config_t base_config(mockingbird_codec_t codec) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend               = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec                 = codec;
    cfg.sample_rate_hz        = 16000;
    cfg.frame_ms              = 20;
    cfg.enable_vad            = 0;
    cfg.enable_anomaly        = 0;
    cfg.packet_queue_capacity = 1024;
    return cfg;
}

Run run(const mockingbird_config_t& cfg, const std::vector<std::vector<int16_t>>& frames) {
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    for (const auto& f : frames) {
        REQUIRE(mockingbird_push_pcm(e, f.data(), 320, ts) == MOCKINGBIRD_OK);
        ts += 20'000ull;
        mockingbird_stats_t st{};
        while (mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK && st.pcm_ring_fill_frames > 1280) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    Run r;
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) {
        Packet p;
        p.segment_id = pkt.segment_id;
        p.flags      = pkt.flags;
        p.codec      = pkt.codec;
        p.bytes.assign(pkt.data, pkt.data + pkt.size);
        r.packets.push_back(std::move(p));
        REQUIRE(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
    }
    mockingbird_event_t evt{};
    while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) {
        if (evt.type == MOCKINGBIRD_EVT_SEGMENT_BEGIN) ++r.segment_begins;
        if (evt.type == MOCKINGBIRD_EVT_SEGMENT_END) ++r.segment_ends;
    }
    mockingbird_destroy(e);
    return r;
}

uint32_t ogg_crc(const std::vector<uint8_t>& page) {
    uint32_t crc = 0;
    for (uint8_t b : page) {
        crc ^= static_cast<uint32_t>(b) << 24;
        for (int k = 0; k < 8; ++k) crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04C11DB7u : crc << 1;
    }
    return crc;
}

uint64_t le(const uint8_t* p, int bytes) {
    uint64_t v = 0;
    for (int i = 0; i < bytes; ++i) v |= static_cast<uint64_t>(p[i]) << (8 * i);
    return v;
}

std::vector<Page> parse(const std::vector<uint8_t>& s) {
    std::vector<Page> pages;
    size_t at = 0;
    while (at < s.size()) {
        REQUIRE(s.size() - at >= 27);
        REQUIRE(std::memcmp(&s[at], "OggS", 4) == 0);
        REQUIRE(s[at + 4] == 0);
        const size_t nseg = s[at + 26];
        REQUIRE(s.size() - at >= 27 + nseg);
        size_t body = 0;
        for (size_t i = 0; i < nseg; ++i) body += s[at + 27 + i];
        const size_t total = 27 + nseg + body;
        REQUIRE(s.size() - at >= total);
        std::vector<uint8_t> raw(s.begin() + static_cast<long>(at),
                                 s.begin() + static_cast<long>(at + total));
        const auto stored = static_cast<uint32_t>(le(&raw[22], 4));
        std::memset(&raw[22], 0, 4);
        CHECK(ogg_crc(raw) == stored);
        Page p;
        p.flags   = s[at + 5];
        p.granule = static_cast<int64_t>(le(&s[at + 6], 8));
        p.serial  = static_cast<uint32_t>(le(&s[at + 14], 4));
        p.seq     = static_cast<uint32_t>(le(&s[at + 18], 4));
        p.body.assign(raw.begin() + static_cast<long>(27 + nseg), raw.end());
        pages.push_back(std::move(p));
        at += total;
    }
    return pages;
}

std::map<uint64_t, std::vector<Packet>> by_segment(const Run& r) {
    std::map<uint64_t, std::vector<Packet>> m;
    for (const auto& p : r.packets) m[p.segment_id].push_back(p);
    return m;
}

void check_last_only_at_end(const std::vector<Packet>& seg) {
    for (size_t i = 0; i + 1 < seg.size(); ++i) {
        CHECK((seg[i].flags & MOCKINGBIRD_PKT_FLAG_LAST) == 0);
    }
    CHECK((seg.back().flags & MOCKINGBIRD_PKT_FLAG_LAST) != 0);
}

}

TEST_CASE("segment_max_ms splits an always-open segment and marks each end LAST") {
    auto cfg           = base_config(MOCKINGBIRD_CODEC_PCM_S16LE);
    cfg.segment_max_ms = 200;
    const Run r = run(cfg, std::vector<std::vector<int16_t>>(35, tone(0.3)));

    const auto segs = by_segment(r);
    REQUIRE(segs.size() == 4);
    CHECK(r.segment_begins == 4);
    CHECK(r.segment_ends == 4);
    size_t n = 0;
    for (const auto& [id, seg] : segs) {
        CHECK(id != 0);
        CHECK((seg.front().flags & MOCKINGBIRD_PKT_FLAG_KEYFRAME) != 0);
        if (n++ < 3) {
            CHECK(seg.size() == 10);
            check_last_only_at_end(seg);
            continue;
        }
        CHECK(seg.size() == 5);
        for (const auto& p : seg) CHECK((p.flags & MOCKINGBIRD_PKT_FLAG_LAST) == 0);
    }
}

TEST_CASE("a segment closed by silence keeps the post-roll and ends on a LAST packet") {
    auto cfg                  = base_config(MOCKINGBIRD_CODEC_PCM_S16LE);
    cfg.enable_anomaly        = 1;
    cfg.anomaly_rms_z         = 2.0f;
    cfg.anomaly_flux_z        = 2.0f;
    cfg.anomaly_background_ms = 2000;
    cfg.vad_hangover_ms       = 300;
    cfg.post_roll_ms          = 600;

    std::vector<std::vector<int16_t>> frames(150, tone(0.001));
    for (size_t i = 100; i < 110; ++i) frames[i] = tone(0.9);
    frames.insert(frames.end(), 60, tone(0.001));
    const Run r = run(cfg, frames);

    const auto segs = by_segment(r);
    REQUIRE(!segs.empty());
    CHECK(r.segment_ends == segs.size());
    for (const auto& [id, seg] : segs) {
        check_last_only_at_end(seg);
        size_t trailing = 0;
        for (auto it = seg.rbegin();
             it != seg.rend() && (it->flags & MOCKINGBIRD_PKT_FLAG_POSTROLL) != 0; ++it) {
            ++trailing;
        }
        CHECK(trailing == 30);
    }
}

#ifdef MOCKINGBIRD_HAVE_OPUS

TEST_CASE("an Ogg Opus segment concatenates into a valid Ogg stream") {
    auto cfg           = base_config(MOCKINGBIRD_CODEC_OGG_OPUS);
    cfg.segment_max_ms = 200;
    const Run r = run(cfg, std::vector<std::vector<int16_t>>(25, tone(0.3)));

    const auto segs = by_segment(r);
    REQUIRE(segs.size() == 3);
    size_t n = 0;
    for (const auto& [id, seg] : segs) {
        const bool cut_by_stop = n++ == 2;
        std::vector<uint8_t> stream;
        for (const auto& p : seg) {
            CHECK(p.codec == MOCKINGBIRD_CODEC_OGG_OPUS);
            stream.insert(stream.end(), p.bytes.begin(), p.bytes.end());
        }
        check_last_only_at_end(seg);
        const auto pages = parse(stream);
        const size_t audio = cut_by_stop ? 5 : 10;
        REQUIRE(pages.size() == 2 + audio + (cut_by_stop ? 1 : 0));

        CHECK(pages[0].flags == 0x02);
        REQUIRE(pages[0].body.size() == 19);
        CHECK(std::memcmp(pages[0].body.data(), "OpusHead", 8) == 0);
        CHECK(pages[0].body[9] == 1);
        CHECK(le(&pages[0].body[12], 4) == 16000);
        CHECK(le(&pages[0].body[10], 2) > 0);
        CHECK(std::memcmp(pages[1].body.data(), "OpusTags", 8) == 0);
        CHECK(pages[0].granule == 0);
        CHECK(pages[1].granule == 0);

        for (size_t i = 0; i < pages.size(); ++i) {
            CHECK(pages[i].serial == static_cast<uint32_t>(id));
            CHECK(pages[i].seq == i);
            CHECK((pages[i].flags & 0x04) == (i + 1 == pages.size() ? 0x04 : 0));
        }
        for (size_t k = 0; k < audio; ++k) {
            CHECK(pages[2 + k].granule == static_cast<int64_t>(960 * (k + 1)));
            CHECK(!pages[2 + k].body.empty());
        }
        if (cut_by_stop) {
            CHECK(pages.back().body.empty());
            CHECK(pages.back().granule == static_cast<int64_t>(960 * audio));
        }
    }
}

TEST_CASE("raw Opus segments split by the cap start on a keyframe and end on LAST") {
    auto cfg = base_config(MOCKINGBIRD_CODEC_OPUS);
    cfg.segment_max_ms = 200;
    const Run r = run(cfg, std::vector<std::vector<int16_t>>(20, tone(0.3)));
    const auto segs = by_segment(r);
    REQUIRE(segs.size() == 2);
    for (const auto& [id, seg] : segs) {
        CHECK((seg.front().flags & MOCKINGBIRD_PKT_FLAG_KEYFRAME) != 0);
        check_last_only_at_end(seg);
    }
}

#endif
