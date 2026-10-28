// Lists capture devices: every HAL device with input streams on macOS, the
// session's available inputs on iOS. Ids are the device or port UID.
#include "capture/coreaudio_backend.hpp"

#if defined(__APPLE__)

#include "capture/coreaudio_impl.hpp"

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#else
#include <CoreAudio/AudioHardware.h>
#endif

namespace mockingbird {
namespace {
#if !TARGET_OS_IPHONE
std::string to_utf8(CFStringRef s) {
    if (s == nullptr) return {};
    char buf[512];
    const bool ok = CFStringGetCString(s, buf, sizeof buf, kCFStringEncodingUTF8);
    CFRelease(s);
    return ok ? std::string(buf) : std::string();
}

CFStringRef copy_string(AudioObjectID object, AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress addr = {selector, kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(object, &addr, 0, nullptr, &size, &value) != noErr) return nullptr;
    return value;
}

bool has_input(AudioObjectID device) {
    AudioObjectPropertyAddress addr = {kAudioDevicePropertyStreams, kAudioObjectPropertyScopeInput,
                                       kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    return AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) == noErr && size > 0;
}

AudioObjectID default_input() {
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDefaultInputDevice,
                                       kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &device);
    return device;
}
#endif

}

mockingbird_status_t coreaudio_list_input_devices(std::vector<InputDevice>& out) {
    out.clear();
#if TARGET_OS_IPHONE
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSString* current = session.currentRoute.inputs.firstObject.UID;
    for (AVAudioSessionPortDescription* port in session.availableInputs) {
        out.push_back({port.UID.UTF8String ? port.UID.UTF8String : "",
                       port.portName.UTF8String ? port.portName.UTF8String : "",
                       current != nil && [port.UID isEqualToString:current]});
    }
    return MOCKINGBIRD_OK;
#else
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size) != noErr) {
        return MOCKINGBIRD_ERR_BACKEND;
    }
    std::vector<AudioObjectID> devices(size / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size,
                                   devices.data()) != noErr) {
        return MOCKINGBIRD_ERR_BACKEND;
    }
    const AudioObjectID fallback = default_input();
    for (AudioObjectID d : devices) {
        if (!has_input(d)) continue;
        out.push_back({to_utf8(copy_string(d, kAudioDevicePropertyDeviceUID)),
                       to_utf8(copy_string(d, kAudioObjectPropertyName)), d == fallback});
    }
    return MOCKINGBIRD_OK;
#endif
}

}

#endif
