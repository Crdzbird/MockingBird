// Opus in an Ogg container, one page per packet. A segment's first packet
// also carries the two header pages, its last packet sets end-of-stream, and
// the stream serial is the segment id, so output stays deterministic.
#include "encode/encoder.hpp"

#include "encode/ogg_opus_headers.hpp"
#include "encode/ogg_page_writer.hpp"

#include <array>
#include <vector>

namespace mockingbird {
namespace {
constexpr uint32_t kGranuleRate = 48000;

class OggOpusEncoder final : public Encoder {
public:
    mockingbird_status_t init(const mockingbird_config_t& cfg) override {
        inner_ = make_opus_encoder();
        if (!inner_) return MOCKINGBIRD_ERR_UNSUPPORTED;
        const mockingbird_status_t s = inner_->init(cfg);
        if (s != MOCKINGBIRD_OK) return s;
        channels_ = static_cast<uint8_t>(cfg.channels);
        rate_ = cfg.sample_rate_hz;
        pre_skip_ = static_cast<uint16_t>(inner_->lookahead_samples() * kGranuleRate / rate_);
        granule_per_frame_ = cfg.frame_ms * (kGranuleRate / 1000u);
        opus_.resize(inner_->max_packet_bytes());
        return MOCKINGBIRD_OK;
    }

    mockingbird_codec_t codec() const override { return MOCKINGBIRD_CODEC_OGG_OPUS; }

    size_t max_packet_bytes() const override {
        return inner_->max_packet_bytes() + OggOpusHeaders::kHeadSize +
               OggOpusHeaders::kTagsSize + 3 * OggPageWriter::kMaxOverhead;
    }

    uint32_t lookahead_samples() const override { return inner_->lookahead_samples(); }

    void begin_frame(const FrameContext& ctx) override {
        ctx_ = ctx;
        inner_->begin_frame(ctx);
    }

    int encode(const int16_t* in, uint32_t n, uint8_t* out, uint32_t cap) override {
        const int r = inner_->encode(in, n, opus_.data(), static_cast<uint32_t>(opus_.size()));
        if (r < 0) return r;
        int off = 0;
        if (ctx_.first) off = write_headers(out, cap);
        if (off < 0) return off;
        granule_ += granule_per_frame_;
        const uint8_t flags = ctx_.last ? OggPageWriter::kEndOfStream : 0;
        const int w = pages_.write(opus_.data(), static_cast<size_t>(r), flags, granule_,
                                   out + off, cap - static_cast<uint32_t>(off));
        if (w < 0) return w;
        open_ = !ctx_.last;
        return off + w;
    }

    int end_segment(uint8_t* out, uint32_t cap) override {
        if (!open_) return 0;
        open_ = false;
        return pages_.write_end(granule_, out, cap);
    }

private:
    int write_headers(uint8_t* out, uint32_t cap) {
        pages_.reset(static_cast<uint32_t>(ctx_.segment_id));
        granule_ = 0;
        std::array<uint8_t, OggOpusHeaders::kHeadSize> head{};
        OggOpusHeaders::head(head.data(), channels_, pre_skip_, rate_);
        std::array<uint8_t, OggOpusHeaders::kTagsSize> tags{};
        OggOpusHeaders::tags(tags.data());
        const int a = pages_.write(head.data(), head.size(), OggPageWriter::kBeginOfStream, 0,
                                   out, cap);
        if (a < 0) return a;
        const int b = pages_.write(tags.data(), tags.size(), 0, 0, out + a,
                                   cap - static_cast<uint32_t>(a));
        return b < 0 ? b : a + b;
    }

    std::unique_ptr<Encoder> inner_;
    OggPageWriter pages_;
    std::vector<uint8_t> opus_;
    FrameContext ctx_;
    uint8_t channels_ = 1;
    uint16_t pre_skip_ = 0;
    uint32_t rate_ = 16000;
    uint32_t granule_per_frame_ = 960;
    int64_t granule_ = 0;
    bool open_ = false;
};

}

std::unique_ptr<Encoder> make_ogg_opus_encoder() {
    return std::make_unique<OggOpusEncoder>();
}

}
