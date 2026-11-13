// Fixed packet slots handed between the host and the audio thread without a
// lock: the host takes a free slot, fills it and passes its index on; the
// audio thread passes it back once the packet has played or been dropped.
#pragma once

#include "mockingbird/mockingbird.h"
#include "spsc_ring.hpp"

#include <cstdint>
#include <memory>

namespace mockingbird {
inline constexpr uint32_t kPlayerMaxPacketBytes = 8192;

struct PacketSlot {
    uint64_t            pts_us     = 0;
    uint64_t            generation = 0;
    uint32_t            flags      = 0;
    uint32_t            size       = 0;
    uint32_t            frames     = 0;
    mockingbird_codec_t codec      = MOCKINGBIRD_CODEC_OPUS;
    uint8_t             bytes[kPlayerMaxPacketBytes];
};

class PacketQueue {
public:
    explicit PacketQueue(uint32_t slots);

    PacketSlot* acquire() noexcept;
    void        submit(PacketSlot* slot) noexcept;
    PacketSlot* take() noexcept;
    void        recycle(PacketSlot* slot) noexcept;

private:
    uint32_t index_of(const PacketSlot* slot) const noexcept {
        return static_cast<uint32_t>(slot - slots_.get());
    }

    std::unique_ptr<PacketSlot[]> slots_;
    SpscRing<uint32_t>            free_;
    SpscRing<uint32_t>            filled_;
};

}
