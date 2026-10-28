// Private state and helpers shared by the Core Audio backend's units.
#pragma once

#include "capture/coreaudio_backend.hpp"

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <TargetConditionals.h>

#include <cstdint>
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
};

mockingbird_status_t coreaudio_open_unit(CoreAudioBackend::Impl& impl, const mockingbird_config_t& cfg);

AudioComponentInstance coreaudio_create_unit();
OSStatus coreaudio_enable_input_only(AudioComponentInstance unit);
OSStatus coreaudio_select_default_input(AudioComponentInstance unit);

OSStatus coreaudio_input_render(void* refCon, AudioUnitRenderActionFlags* ioActionFlags,
                                const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                UInt32 inNumberFrames, AudioBufferList* ioData);

}
