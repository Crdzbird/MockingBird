// Creates and configures the Apple output unit: RemoteIO on iOS, the
// default output unit on macOS, interleaved int16 at the player's rate. The
// render callback hands the unit's buffer straight to the player.
#include "play/coreaudio_output.hpp"

#if defined(__APPLE__)

#include "play/coreaudio_output_impl.hpp"

namespace mockingbird {
static OSStatus render(void* ref, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 frames,
                AudioBufferList* io) {
    auto* impl = static_cast<CoreAudioOutput::Impl*>(ref);
    if (io == nullptr || io->mNumberBuffers < 1 || io->mBuffers[0].mData == nullptr) return noErr;
    const UInt32 need = frames * impl->channels * sizeof(int16_t);
    if (io->mBuffers[0].mDataByteSize < need) return kAudioUnitErr_TooManyFramesToProcess;
    impl->cb(impl->user, static_cast<int16_t*>(io->mBuffers[0].mData), frames);
    return noErr;
}

AudioComponentInstance coreaudio_output_create_unit() {
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    AudioComponentInstance unit = nullptr;
    if (comp == nullptr || AudioComponentInstanceNew(comp, &unit) != noErr) return nullptr;
    return unit;
}

OSStatus coreaudio_output_configure(AudioComponentInstance unit, const OutputFormat& fmt, CoreAudioOutput::Impl* impl) {
    AudioStreamBasicDescription asbd{};
    asbd.mSampleRate       = fmt.sample_rate_hz;
    asbd.mFormatID         = kAudioFormatLinearPCM;
    asbd.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked |
                             kAudioFormatFlagsNativeEndian;
    asbd.mFramesPerPacket  = 1;
    asbd.mChannelsPerFrame = fmt.channels;
    asbd.mBitsPerChannel   = 16;
    asbd.mBytesPerFrame    = static_cast<UInt32>(fmt.channels) * 2u;
    asbd.mBytesPerPacket   = asbd.mBytesPerFrame;
    OSStatus s = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                      &asbd, sizeof(asbd));
    AURenderCallbackStruct cb{render, impl};
    if (s == noErr) {
        s = AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                                 &cb, sizeof(cb));
    }
    return s == noErr ? AudioUnitInitialize(unit) : s;
}

}

#endif
