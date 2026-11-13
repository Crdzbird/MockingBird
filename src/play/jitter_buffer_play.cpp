// Playout side of the jitter buffer. Each call yields one step: silence while
// waiting, the next packet when it is due, concealment for a short hole, a
// fade on underrun, and a new talkspurt after LAST or a long hole.
#include "play/jitter_buffer.hpp"

namespace mockingbird {
bool JitterBuffer::ready(uint64_t now) const noexcept {
    if (cfg_.target_frames == 0 || buffered_ >= cfg_.target_frames) return true;
    for (uint32_t i = 0; i < n_; ++i) {
        if (now - entries_[i].arrival >= cfg_.target_frames) return true;
        if ((entries_[i].slot->flags & MOCKINGBIRD_PKT_FLAG_LAST) != 0) return true;
    }
    return false;
}

Step JitterBuffer::play_front() noexcept {
    PacketSlot* s = entries_[0].slot;
    remove_at(0);
    cursor_us_   = end_us(s);
    last_frames_ = s->frames;
    if ((s->flags & MOCKINGBIRD_PKT_FLAG_LAST) != 0) {
        phase_      = Phase::Idle;
        has_cursor_ = false;
    }
    return Step{StepKind::Play, s, s->frames};
}

Step JitterBuffer::next(uint64_t now, uint32_t want) noexcept {
    const Step silence{StepKind::Silence, nullptr, want};
    for (int pass = 0; pass < 2; ++pass) {
        if (phase_ != Phase::Playing) {
            if (n_ == 0 || !ready(now)) {
                if (n_ > 0 || has_cursor_) phase_ = Phase::Buffering;
                return silence;
            }
            if (!has_cursor_) cursor_us_ = entries_[0].slot->pts_us;
            has_cursor_ = true;
            phase_      = Phase::Playing;
        }
        if (n_ == 0) {
            shared_->count(shared_->underruns);
            phase_ = Phase::Buffering;
            return last_frames_ != 0 ? Step{StepKind::Fade, nullptr, last_frames_} : silence;
        }
        const PacketSlot* front = entries_[0].slot;
        const int64_t gap = to_frames(static_cast<int64_t>(front->pts_us - cursor_us_));
        if (gap * 2 < static_cast<int64_t>(front->frames)) return play_front();
        if (gap <= static_cast<int64_t>(cfg_.conceal_frames)) {
            const uint32_t grid = cfg_.sample_rate_hz / 400u;
            uint32_t f = last_frames_ != 0 ? last_frames_ : front->frames;
            if (static_cast<int64_t>(f) > gap) f = static_cast<uint32_t>(gap) / grid * grid;
            if (f == 0) return play_front();
            cursor_us_ += uint64_t{f} * 1'000'000u / cfg_.sample_rate_hz;
            shared_->count(shared_->concealed);
            return Step{StepKind::Conceal, nullptr, f};
        }
        phase_      = Phase::Idle;
        has_cursor_ = false;
    }
    return silence;
}

}
