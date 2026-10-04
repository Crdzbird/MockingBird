// Creates the platform input AudioUnit (RemoteIO on iOS, HALOutput on macOS)
// and binds it to the system's default input device.
#include "capture/coreaudio_backend.hpp"

#if defined(__APPLE__)

#include "capture/coreaudio_impl.hpp"

#if !TARGET_OS_IPHONE
#include <CoreAudio/AudioHardware.h>
#endif

namespace auris {

AudioComponentInstance coreaudio_create_unit() {
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    desc.componentSubType = kAudioUnitSubType_HALOutput;
#endif
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (comp == nullptr) return nullptr;
    AudioComponentInstance unit = nullptr;
    if (AudioComponentInstanceNew(comp, &unit) != noErr) return nullptr;
    return unit;
}

OSStatus coreaudio_enable_input_only(AudioComponentInstance unit) {
    UInt32 one  = 1;
    UInt32 zero = 0;
    OSStatus s = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO,
                                      kAudioUnitScope_Input, kCoreAudioInputBus,
                                      &one, sizeof(one));
    if (s != noErr) return s;
    return AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output, kCoreAudioOutputBus,
                                &zero, sizeof(zero));
}

OSStatus coreaudio_select_default_input(AudioComponentInstance unit) {
#if TARGET_OS_IPHONE
    (void)unit;
    return noErr;
#else
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDefaultInputDevice,
                                       kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    OSStatus s = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr,
                                            &size, &device);
    if (s != noErr) return s;
    if (device == kAudioObjectUnknown) return kAudioUnitErr_InvalidProperty;
    return AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global, kCoreAudioOutputBus,
                                &device, sizeof(device));
#endif
}

} // namespace auris

#endif
