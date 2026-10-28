// Private state and helpers shared by the Core Audio backend's units.
#pragma once

#include "capture/coreaudio_backend.hpp"

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <TargetConditionals.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace mockingbird {
constexpr AudioUnitElement kCoreAudioInputBus  = 1;
constexpr AudioUnitElement kCoreAudioOutputBus = 0;

struct CoreAudioBackend::Impl {
    AudioComponentInstance unit        = nullptr;
    bool                   initialized = false;
    bool                   running     = false;
    PcmCallback            cb          = nullptr;
    void*                  user        = nullptr;
    uint32_t               sample_rate = 0;
    uint16_t               channels    = 1;
    std::vector<int16_t>   scratch;

    CaptureNoticeCallback  notice          = nullptr;
    void*                  notice_user     = nullptr;
    std::string            device_id;
    uint32_t               voice_requested = 0;
    uint32_t               voice_active    = 0;
    std::mutex             route_mu;
    bool                   watching        = false;
    void*                  observers[3]    = {nullptr, nullptr, nullptr};

    void tell(mockingbird_event_type_t type) {
        if (notice != nullptr) notice(notice_user, type, 0);
    }
};

mockingbird_status_t coreaudio_open_unit(CoreAudioBackend::Impl& impl, const mockingbird_config_t& cfg);

AudioComponentInstance coreaudio_create_unit(bool voice_processing);
OSStatus coreaudio_enable_input_only(AudioComponentInstance unit);
OSStatus coreaudio_play_silence(AudioComponentInstance unit, const AudioStreamBasicDescription& fmt);
OSStatus coreaudio_select_input(AudioComponentInstance unit, const std::string& device_id,
                                AudioUnitElement element);
uint32_t coreaudio_configure_voice(AudioComponentInstance unit, uint32_t requested);

void coreaudio_watch_routes(CoreAudioBackend::Impl& impl);
void coreaudio_unwatch_routes(CoreAudioBackend::Impl& impl);

OSStatus coreaudio_input_render(void* refCon, AudioUnitRenderActionFlags* ioActionFlags,
                                const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                UInt32 inNumberFrames, AudioBufferList* ioData);

}
