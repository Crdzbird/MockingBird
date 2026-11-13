// Arrival side of the jitter buffer: ordered insertion, late and duplicate
// drops, the timeline reset for a restarted sender, and trimming a backlog
// that has grown past max_delay_ms back down to the target.
#include "play/jitter_buffer.hpp"

namespace mockingbird {
namespace {
constexpr uint64_t kResetBehindUs = 2'000'000;
}

void JitterBuffer::init(const JitterSettings& s, PacketQueue* queue, PlayerShared* shared) {
    cfg_     = s;
    queue_   = queue;
    shared_  = shared;
    entries_ = std::make_unique<Entry[]>(s.capacity);
}

int64_t JitterBuffer::to_frames(int64_t us) const noexcept {
    return us * static_cast<int64_t>(cfg_.sample_rate_hz) / 1'000'000;
}

uint64_t JitterBuffer::end_us(const PacketSlot* s) const noexcept {
    return s->pts_us + uint64_t{s->frames} * 1'000'000u / cfg_.sample_rate_hz;
}

bool JitterBuffer::insert(PacketSlot* slot, uint64_t now) noexcept {
    bool reset = false;
    const uint64_t half_us = uint64_t{slot->frames} * 500'000u / cfg_.sample_rate_hz;
    if (has_cursor_ && slot->pts_us + kResetBehindUs < cursor_us_) {
        clear();
        reset = true;
    }
    if (has_cursor_ && slot->pts_us + half_us < cursor_us_) {
        shared_->count(shared_->dropped);
        shared_->count(shared_->late);
        queue_->recycle(slot);
        return reset;
    }
    uint32_t at = n_;
    while (at > 0 && entries_[at - 1].slot->pts_us > slot->pts_us) --at;
    const bool dup_before = at > 0 && entries_[at - 1].slot->pts_us + half_us > slot->pts_us;
    const bool dup_after  = at < n_ && slot->pts_us + half_us > entries_[at].slot->pts_us;
    if (dup_before || dup_after || n_ == cfg_.capacity) {
        shared_->count(shared_->dropped);
        if (n_ != cfg_.capacity) shared_->count(shared_->late);
        queue_->recycle(slot);
        return reset;
    }
    for (uint32_t i = n_; i > at; --i) entries_[i] = entries_[i - 1];
    entries_[at] = Entry{slot, now};
    ++n_;
    buffered_ += slot->frames;
    trim();
    return reset;
}

void JitterBuffer::remove_at(uint32_t i) noexcept {
    buffered_ -= entries_[i].slot->frames;
    for (uint32_t k = i + 1; k < n_; ++k) entries_[k - 1] = entries_[k];
    --n_;
}

void JitterBuffer::drop_front(bool late) noexcept {
    PacketSlot* s = entries_[0].slot;
    if (has_cursor_ && end_us(s) > cursor_us_) cursor_us_ = end_us(s);
    remove_at(0);
    shared_->count(shared_->dropped);
    if (late) shared_->count(shared_->late);
    queue_->recycle(s);
}

void JitterBuffer::trim() noexcept {
    if (buffered_ <= cfg_.max_frames) return;
    while (n_ > 1 && buffered_ > cfg_.target_frames) drop_front(false);
}

void JitterBuffer::clear() noexcept {
    for (uint32_t i = 0; i < n_; ++i) queue_->recycle(entries_[i].slot);
    n_           = 0;
    buffered_    = 0;
    last_frames_ = 0;
    phase_       = Phase::Idle;
    has_cursor_  = false;
}

mockingbird_player_state_t JitterBuffer::state() const noexcept {
    switch (phase_) {
        case Phase::Playing:   return MOCKINGBIRD_PLAYER_PLAYING;
        case Phase::Buffering: return MOCKINGBIRD_PLAYER_BUFFERING;
        case Phase::Idle:      break;
    }
    return n_ > 0 ? MOCKINGBIRD_PLAYER_BUFFERING : MOCKINGBIRD_PLAYER_IDLE;
}

}
