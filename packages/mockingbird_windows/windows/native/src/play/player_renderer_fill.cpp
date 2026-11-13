// Turns one jitter step into samples in the carry buffer, and copies the
// carry out through the volume ramp. A packet that fails to decode is
// dropped and concealed in place, so the timeline never stalls.
#include "play/player_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mockingbird {
void PlayerRenderer::fill(uint32_t want) noexcept {
    const Step step = jitter_.next(clock_, std::min(want, max_frames_));
    carry_at_ = 0;
    switch (step.kind) {
        case StepKind::Play:
            decode(step.slot);
            queue_->recycle(step.slot);
            return;
        case StepKind::Conceal:
        case StepKind::Fade:
            conceal(step.frames);
            return;
        case StepKind::Silence:
            break;
    }
    carry_frames_ = std::max(step.frames, 1u);
    std::memset(carry_.get(), 0, size_t{carry_frames_} * channels_ * sizeof(int16_t));
}

void PlayerRenderer::decode(PacketSlot* slot) noexcept {
    if (slot->codec == MOCKINGBIRD_CODEC_PCM_S16LE) {
        carry_frames_ = slot->frames;
        std::memcpy(carry_.get(), slot->bytes, size_t{slot->frames} * channels_ * sizeof(int16_t));
        opus_history_ = false;
        shared_->count(shared_->played);
        return;
    }
    uint32_t n = 0;
    if (opus_.decode(slot->bytes, slot->size, carry_.get(), max_frames_, n) != MOCKINGBIRD_OK) {
        shared_->count(shared_->dropped);
        conceal(slot->frames);
        return;
    }
    carry_frames_ = n;
    opus_history_ = true;
    shared_->count(shared_->played);
}

void PlayerRenderer::conceal(uint32_t frames) noexcept {
    const uint32_t f = std::clamp(frames, 1u, max_frames_);
    uint32_t n = 0;
    if (opus_history_ && opus_.conceal(f, carry_.get(), max_frames_, n) == MOCKINGBIRD_OK && n > 0) {
        carry_frames_ = n;
        return;
    }
    carry_frames_ = f;
    std::memset(carry_.get(), 0, size_t{f} * channels_ * sizeof(int16_t));
}

void PlayerRenderer::copy_out(int16_t* out, uint32_t frames) noexcept {
    const float target = shared_->muted.load(std::memory_order_relaxed)
        ? 0.0f
        : shared_->volume.load(std::memory_order_relaxed);
    const float step = 100.0f / static_cast<float>(rate_);
    if (gain_ < 0.0f) gain_ = target;
    const int16_t* src = carry_.get() + size_t{carry_at_} * channels_;
    for (uint32_t i = 0; i < frames; ++i) {
        if (gain_ < target) gain_ = std::min(target, gain_ + step);
        if (gain_ > target) gain_ = std::max(target, gain_ - step);
        for (uint16_t c = 0; c < channels_; ++c) {
            const size_t k = size_t{i} * channels_ + c;
            const float v = std::round(static_cast<float>(src[k]) * gain_);
            out[k] = static_cast<int16_t>(std::clamp(v, -32768.0f, 32767.0f));
        }
    }
}

}
