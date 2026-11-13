// The free ring starts holding every slot index; push moves an index from
// free to filled and the audio thread moves it back.
#include "play/packet_queue.hpp"

namespace mockingbird {
PacketQueue::PacketQueue(uint32_t slots)
    : slots_(std::make_unique<PacketSlot[]>(slots)), free_(slots), filled_(slots) {
    for (uint32_t i = 0; i < slots; ++i) free_.push(&i, 1);
}

PacketSlot* PacketQueue::acquire() noexcept {
    uint32_t i = 0;
    return free_.pop(&i, 1) == 1 ? &slots_[i] : nullptr;
}

void PacketQueue::submit(PacketSlot* slot) noexcept {
    const uint32_t i = index_of(slot);
    filled_.push(&i, 1);
}

PacketSlot* PacketQueue::take() noexcept {
    uint32_t i = 0;
    return filled_.pop(&i, 1) == 1 ? &slots_[i] : nullptr;
}

void PacketQueue::recycle(PacketSlot* slot) noexcept {
    const uint32_t i = index_of(slot);
    free_.push(&i, 1);
}

}
