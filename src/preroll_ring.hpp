// Fixed-capacity ring of recent frames, kept while no segment is open so a
// detection can be emitted with the audio that preceded its trigger.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace mockingbird {
class PrerollRing {
public:
    struct Frame {
        std::vector<int16_t> samples;
        uint64_t             capture_ts_us = 0;
    };

    void init(size_t capacity_frames, size_t frame_samples) {
        ring_.clear();
        ring_.resize(capacity_frames);
        for (auto& f : ring_) f.samples.assign(frame_samples, 0);
        frame_samples_ = frame_samples;
        head_   = 0;
        filled_ = 0;
    }

    bool   empty() const noexcept { return filled_ == 0; }
    size_t size() const noexcept { return filled_; }
    size_t capacity() const noexcept { return ring_.size(); }

    void clear() noexcept {
        head_   = 0;
        filled_ = 0;
    }

    void push(const int16_t* samples, uint64_t capture_ts_us) noexcept {
        if (ring_.empty() || frame_samples_ == 0) return;
        auto& slot = ring_[head_];
        std::memcpy(slot.samples.data(), samples, frame_samples_ * sizeof(int16_t));
        slot.capture_ts_us = capture_ts_us;
        head_ = (head_ + 1) % ring_.size();
        if (filled_ < ring_.size()) ++filled_;
    }

    template <typename F>
    void for_each_oldest_first(F&& f) const {
        if (filled_ == 0) return;
        const size_t start = (head_ + ring_.size() - filled_) % ring_.size();
        for (size_t i = 0; i < filled_; ++i) f(ring_[(start + i) % ring_.size()]);
    }

    uint64_t oldest_ts_us() const noexcept {
        if (filled_ == 0) return 0;
        return ring_[(head_ + ring_.size() - filled_) % ring_.size()].capture_ts_us;
    }

private:
    std::vector<Frame> ring_;
    size_t             frame_samples_ = 0;
    size_t             head_          = 0;
    size_t             filled_        = 0;
};

}
