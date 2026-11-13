// The player behind the opaque mockingbird_player_t handle: the host-side
// slot queue, the audio-thread renderer, the shared counters and the output.
#pragma once

#include "mockingbird/mockingbird.h"
#include "play/output_backend.hpp"
#include "play/packet_queue.hpp"
#include "play/player_renderer.hpp"
#include "play/player_shared.hpp"

#include <memory>
#include <mutex>

struct mockingbird_player {
    explicit mockingbird_player(const mockingbird_player_config_t& c)
        : cfg(c), queue(c.queue_packets) {}

    mockingbird_player_config_t                   cfg;
    mockingbird::PlayerShared                     shared;
    mockingbird::PacketQueue                      queue;
    mockingbird::PlayerRenderer                   renderer;
    std::unique_ptr<mockingbird::OutputBackend>   output;
    std::mutex                                    lifecycle_mu;
};

namespace mockingbird {
mockingbird_status_t validate_player_config(const mockingbird_player_config_t& c);
mockingbird_status_t open_player_output(mockingbird_player* p);
uint32_t packet_frames_for(const mockingbird_player_config_t& c, int codec, const uint8_t* data,
                           uint32_t size, mockingbird_status_t& status);
}
