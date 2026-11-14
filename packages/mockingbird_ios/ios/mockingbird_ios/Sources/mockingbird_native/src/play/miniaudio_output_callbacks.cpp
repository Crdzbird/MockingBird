// miniaudio playback callbacks. The data callback runs on miniaudio's audio
// thread and only calls the player's renderer; notifications become output
// notices, and a stop the player did not ask for counts as an interruption
// and leaves the device stopped, so a resume starts it again.
#ifndef __EMSCRIPTEN__

#include "play/miniaudio_output_impl.hpp"

namespace mockingbird {
void miniaudio_output_data(ma_device* device, void* output, const void*, ma_uint32 frames) {
    auto* impl = static_cast<MiniaudioOutput::Impl*>(device->pUserData);
    if (impl == nullptr || output == nullptr || frames == 0) return;
    impl->cb(impl->user, static_cast<int16_t*>(output), frames);
}

void miniaudio_output_notice(const ma_device_notification* n) {
    auto* impl = static_cast<MiniaudioOutput::Impl*>(n->pDevice->pUserData);
    if (impl == nullptr) return;
    switch (n->type) {
        case ma_device_notification_type_rerouted:           impl->tell(OutputNotice::RouteChanged); break;
        case ma_device_notification_type_interruption_began: impl->tell(OutputNotice::Interrupted); break;
        case ma_device_notification_type_interruption_ended: impl->tell(OutputNotice::Resumed); break;
        case ma_device_notification_type_stopped:
            if (impl->stopping.load(std::memory_order_acquire)) break;
            impl->running.store(false, std::memory_order_release);
            impl->tell(OutputNotice::Interrupted);
            break;
        default: break;
    }
}

mockingbird_status_t miniaudio_output_status(ma_result r) {
    if (r == MA_SUCCESS) return MOCKINGBIRD_OK;
    if (r == MA_OUT_OF_MEMORY) return MOCKINGBIRD_ERR_NO_MEMORY;
    return r == MA_ACCESS_DENIED ? MOCKINGBIRD_ERR_PERMISSION : MOCKINGBIRD_ERR_BACKEND;
}

}

#endif
