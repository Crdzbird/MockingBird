// Private state of the miniaudio output and the callbacks miniaudio calls.
#pragma once

#include "play/miniaudio_output.hpp"

#include "miniaudio.h"

#include <atomic>

namespace mockingbird {
struct MiniaudioOutput::Impl {
    ma_device            device{};
    bool                 initialized = false;
    std::atomic<bool>    running{false};
    std::atomic<bool>    stopping{false};
    RenderCallback       cb          = nullptr;
    void*                user        = nullptr;
    OutputNoticeCallback notice      = nullptr;
    void*                notice_user = nullptr;

    void tell(OutputNotice n) const {
        if (notice != nullptr) notice(notice_user, n);
    }
};

void miniaudio_output_data(ma_device* device, void* output, const void* input, ma_uint32 frames);
void miniaudio_output_notice(const ma_device_notification* n);
mockingbird_status_t miniaudio_output_status(ma_result r);

}
