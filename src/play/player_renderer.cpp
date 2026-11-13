// The audio-thread loop: apply a pending flush, take newly pushed packets,
// then fill the output from the carry buffer, refilling it one jitter step
// at a time.
#include "play/player_renderer.hpp"

#include <algorithm>

namespace mockingbird {
namespace {
uint32_t ms_to_frames(uint32_t ms, uint32_t rate) {
    return static_cast<uint32_t>(uint64_t{ms} * rate / 1000u);
}

}

mockingbird_status_t PlayerRenderer::init(const mockingbird_player_config_t& cfg, PacketQueue* queue,
                                          PlayerShared* shared) {
    queue_      = queue;
    shared_     = shared;
    rate_       = cfg.sample_rate_hz;
    channels_   = cfg.channels;
    max_frames_ = OpusFrameDecoder::max_frames_for(rate_);
    carry_      = std::make_unique<int16_t[]>(size_t{max_frames_} * channels_);
    JitterSettings s;
    s.sample_rate_hz = rate_;
    s.target_frames  = ms_to_frames(cfg.target_delay_ms, rate_);
    s.max_frames     = ms_to_frames(cfg.max_delay_ms, rate_);
    s.conceal_frames = ms_to_frames(cfg.conceal_max_ms, rate_);
    s.capacity       = cfg.queue_packets;
    jitter_.init(s, queue, shared);
    const mockingbird_status_t opus = opus_.init(rate_, channels_);
    return opus == MOCKINGBIRD_ERR_UNSUPPORTED ? MOCKINGBIRD_OK : opus;
}

void PlayerRenderer::restart() noexcept {
    jitter_.clear();
    opus_.reset();
    opus_history_ = false;
    carry_frames_ = 0;
    carry_at_     = 0;
}

void PlayerRenderer::sync() noexcept {
    const uint64_t wanted = shared_->generation.load(std::memory_order_acquire);
    if (wanted != generation_) {
        restart();
        generation_ = wanted;
    }
    while (PacketSlot* s = queue_->take()) {
        if (s->generation < generation_) {
            queue_->recycle(s);
            continue;
        }
        if (s->generation > generation_) {
            restart();
            generation_ = s->generation;
        }
        if (jitter_.insert(s, clock_)) {
            opus_.reset();
            opus_history_ = false;
        }
    }
}

void PlayerRenderer::render(int16_t* out, uint32_t frames) noexcept {
    sync();
    uint32_t done = 0;
    while (done < frames) {
        if (carry_at_ == carry_frames_) fill(frames - done);
        const uint32_t n = std::min(frames - done, carry_frames_ - carry_at_);
        copy_out(out + size_t{done} * channels_, n);
        carry_at_ += n;
        done += n;
    }
    clock_ += frames;
    shared_->frames_rendered.fetch_add(frames, std::memory_order_relaxed);
    publish();
}

void PlayerRenderer::publish() noexcept {
    shared_->buffered_packets.store(jitter_.size(), std::memory_order_relaxed);
    shared_->buffered_ms.store(static_cast<uint32_t>(uint64_t{jitter_.buffered_frames()} * 1000u / rate_),
                               std::memory_order_relaxed);
    shared_->render_state.store(static_cast<uint32_t>(jitter_.state()), std::memory_order_relaxed);
}

}
