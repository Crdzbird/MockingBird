// Private state of the Core Audio output, shared with the route and
// interruption observers, which hold it alive and check `alive` under the
// lock before touching the unit.
#pragma once

#include "play/coreaudio_output.hpp"

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <TargetConditionals.h>

#include <memory>
#include <mutex>

namespace mockingbird {
struct CoreAudioOutput::Impl {
    AudioComponentInstance unit         = nullptr;
    RenderCallback         cb           = nullptr;
    void*                  user         = nullptr;
    OutputNoticeCallback   notice       = nullptr;
    void*                  notice_user  = nullptr;
    uint16_t               channels     = 1;
    std::mutex             mu;
    bool                   alive        = true;
    bool                   wanted       = false;
    bool                   running      = false;
    bool                   session_held = false;
    void*                  observers[3] = {nullptr, nullptr, nullptr};

    void tell(OutputNotice n) const {
        if (notice != nullptr) notice(notice_user, n);
    }
    bool start_unit();
    void stop_unit();
};

void coreaudio_output_watch(const std::shared_ptr<CoreAudioOutput::Impl>& impl);
void coreaudio_output_unwatch(CoreAudioOutput::Impl& impl);

}
