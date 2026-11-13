// The audio thread's ordering of what has arrived: packets sorted by pts, the
// playout point, and the choices the C ABI documents (wait, play, conceal,
// drop, underrun). It only decides; the renderer turns a step into samples.
#pragma once

#include "play/packet_queue.hpp"
#include "play/player_shared.hpp"

#include <cstdint>
#include <memory>

namespace mockingbird {
struct JitterSettings {
    uint32_t sample_rate_hz = 16000;
    uint32_t target_frames  = 0;
    uint32_t max_frames     = 0;
    uint32_t conceal_frames = 0;
    uint32_t capacity       = 0;
};

enum class StepKind { Silence, Play, Conceal, Fade };

struct Step {
    StepKind    kind   = StepKind::Silence;
    PacketSlot* slot   = nullptr;
    uint32_t    frames = 0;
};

class JitterBuffer {
public:
    void init(const JitterSettings& s, PacketQueue* queue, PlayerShared* shared);
    bool insert(PacketSlot* slot, uint64_t now) noexcept;
    Step next(uint64_t now, uint32_t want) noexcept;
    void clear() noexcept;

    uint32_t size() const noexcept { return n_; }
    uint32_t buffered_frames() const noexcept { return buffered_; }
    mockingbird_player_state_t state() const noexcept;

private:
    struct Entry {
        PacketSlot* slot    = nullptr;
        uint64_t    arrival = 0;
    };
    enum class Phase { Idle, Buffering, Playing };

    bool ready(uint64_t now) const noexcept;
    Step play_front() noexcept;
    void drop_front(bool late) noexcept;
    void trim() noexcept;
    void remove_at(uint32_t i) noexcept;
    int64_t to_frames(int64_t us) const noexcept;
    uint64_t end_us(const PacketSlot* s) const noexcept;

    JitterSettings           cfg_{};
    PacketQueue*             queue_  = nullptr;
    PlayerShared*            shared_ = nullptr;
    std::unique_ptr<Entry[]> entries_;
    uint32_t                 n_           = 0;
    uint32_t                 buffered_    = 0;
    uint32_t                 last_frames_ = 0;
    Phase                    phase_       = Phase::Idle;
    bool                     has_cursor_  = false;
    uint64_t                 cursor_us_   = 0;
};

}
