// State the host side and the audio thread share: the flush generation,
// volume and mute, and the counters behind mockingbird_player_get_stats.
// Everything is atomic, so either side reads and writes without a lock.
#pragma once

#include <atomic>
#include <cstdint>

namespace mockingbird {
struct PlayerShared {
    std::atomic<uint64_t> generation{0};
    std::atomic<float>    volume{1.0f};
    std::atomic<bool>     muted{false};
    std::atomic<bool>     started{false};
    std::atomic<bool>     paused{false};
    std::atomic<bool>     interrupted{false};
    std::atomic<bool>     route_paused{false};

    std::atomic<uint64_t> received{0};
    std::atomic<uint64_t> played{0};
    std::atomic<uint64_t> concealed{0};
    std::atomic<uint64_t> dropped{0};
    std::atomic<uint64_t> late{0};
    std::atomic<uint64_t> underruns{0};
    std::atomic<uint64_t> frames_rendered{0};
    std::atomic<uint32_t> buffered_packets{0};
    std::atomic<uint32_t> buffered_ms{0};
    std::atomic<uint32_t> render_state{0};
    std::atomic<uint32_t> output_restarts{0};

    void count(std::atomic<uint64_t>& c) noexcept { c.fetch_add(1, std::memory_order_relaxed); }
};

}
