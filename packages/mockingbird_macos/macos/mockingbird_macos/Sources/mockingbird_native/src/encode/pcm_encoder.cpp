// Passthrough encoder: copies int16 little-endian samples unchanged.
#include "encode/encoder.hpp"

#include <cstring>

namespace mockingbird {

namespace {

class PassthroughEncoder final : public Encoder {
public:
    mockingbird_status_t init(const mockingbird_config_t& cfg) override {
        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        max_bytes_     = static_cast<size_t>(frame_samples_) * sizeof(int16_t);
        return MOCKINGBIRD_OK;
    }

    mockingbird_codec_t codec() const override { return MOCKINGBIRD_CODEC_PCM_S16LE; }
    size_t max_packet_bytes() const override { return max_bytes_; }

    int encode(const int16_t* in_samples, uint32_t n, uint8_t* out_buf,
               uint32_t out_cap) override {
        if (n != frame_samples_) return -1;
        const uint32_t bytes = n * static_cast<uint32_t>(sizeof(int16_t));
        if (out_cap < bytes) return -1;
        std::memcpy(out_buf, in_samples, bytes);
        return static_cast<int>(bytes);
    }

private:
    uint32_t frame_samples_ = 0;
    size_t   max_bytes_     = 0;
};

} // namespace

std::unique_ptr<Encoder> make_passthrough_encoder() {
    return std::make_unique<PassthroughEncoder>();
}

} // namespace mockingbird
