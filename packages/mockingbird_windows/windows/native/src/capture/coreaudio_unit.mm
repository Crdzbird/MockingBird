// Creates the platform input AudioUnit: RemoteIO on iOS and HALOutput on
// macOS, or VoiceProcessingIO on both when voice processing is asked for.
// Binds it to the chosen input, or the system default.
#include "capture/coreaudio_backend.hpp"

#if defined(__APPLE__)

#include "capture/coreaudio_impl.hpp"

#include <cstring>

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#else
#include <CoreAudio/AudioHardware.h>
#endif

namespace mockingbird {
AudioComponentInstance coreaudio_create_unit(bool voice_processing) {
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    desc.componentSubType = kAudioUnitSubType_HALOutput;
#endif
    if (voice_processing) desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (comp == nullptr) return nullptr;
    AudioComponentInstance unit = nullptr;
    if (AudioComponentInstanceNew(comp, &unit) != noErr) return nullptr;
    return unit;
}

namespace {
OSStatus render_silence(void*, AudioUnitRenderActionFlags* flags, const AudioTimeStamp*, UInt32,
                        UInt32, AudioBufferList* io) {
    for (UInt32 i = 0; i < io->mNumberBuffers; ++i) {
        std::memset(io->mBuffers[i].mData, 0, io->mBuffers[i].mDataByteSize);
    }
    *flags |= kAudioUnitRenderAction_OutputIsSilence;
    return noErr;
}

}

OSStatus coreaudio_play_silence(AudioComponentInstance unit, const AudioStreamBasicDescription& fmt) {
    OSStatus s = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                      kCoreAudioOutputBus, &fmt, sizeof(fmt));
    if (s != noErr) return s;
    AURenderCallbackStruct cb{render_silence, nullptr};
    return AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                kCoreAudioOutputBus, &cb, sizeof(cb));
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

uint32_t coreaudio_configure_voice(AudioComponentInstance unit, uint32_t requested) {
    if (requested == 0) return 0;
    UInt32 bypass = 0;
    AudioUnitSetProperty(unit, kAUVoiceIOProperty_BypassVoiceProcessing, kAudioUnitScope_Global,
                         kCoreAudioInputBus, &bypass, sizeof(bypass));
    UInt32 agc = (requested & MOCKINGBIRD_VOICE_GAIN_CONTROL) != 0 ? 1 : 0;
    const OSStatus s = AudioUnitSetProperty(unit, kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                                            kAudioUnitScope_Global, kCoreAudioInputBus,
                                            &agc, sizeof(agc));
    uint32_t active = MOCKINGBIRD_VOICE_ECHO_CANCEL | MOCKINGBIRD_VOICE_NOISE_SUPPRESS;
    if (s == noErr && agc != 0) active |= MOCKINGBIRD_VOICE_GAIN_CONTROL;
    return active;
}

OSStatus coreaudio_select_input(AudioComponentInstance unit, const std::string& device_id,
                                AudioUnitElement element) {
#if TARGET_OS_IPHONE
    (void)unit;
    (void)element;
    AVAudioSession* session = [AVAudioSession sharedInstance];
    if (device_id.empty()) {
        [session setPreferredInput:nil error:nil];
        return noErr;
    }
    NSString* wanted = [NSString stringWithUTF8String:device_id.c_str()];
    for (AVAudioSessionPortDescription* port in session.availableInputs) {
        if ([port.UID isEqualToString:wanted]) {
            return [session setPreferredInput:port error:nil] ? noErr : kAudioUnitErr_InvalidProperty;
        }
    }
    return kAudioUnitErr_InvalidProperty;
#else
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    if (device_id.empty()) {
        AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDefaultInputDevice,
                                           kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain};
        OSStatus s = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr,
                                                &size, &device);
        if (s != noErr) return s;
    } else {
        CFStringRef uid = CFStringCreateWithCString(nullptr, device_id.c_str(), kCFStringEncodingUTF8);
        AudioObjectPropertyAddress addr = {kAudioHardwarePropertyTranslateUIDToDevice,
                                           kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain};
        OSStatus s = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, sizeof(uid), &uid,
                                                &size, &device);
        if (uid != nullptr) CFRelease(uid);
        if (s != noErr) return s;
    }
    if (device == kAudioObjectUnknown) return kAudioUnitErr_InvalidProperty;
    return AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global, element, &device, sizeof(device));
#endif
}

}

#endif
