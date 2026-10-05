// Core Audio input render callback. Runs on the OS audio thread, so it must
// not allocate after the first call or log.
#include "capture/coreaudio_backend.hpp"

#if defined(__APPLE__)

#include "capture/coreaudio_impl.hpp"
#include "capture/mach_timebase.hpp"

namespace mockingbird {

OSStatus coreaudio_input_render(void* refCon, AudioUnitRenderActionFlags* ioActionFlags,
                                const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                UInt32 inNumberFrames, AudioBufferList*) {
    auto* impl = static_cast<CoreAudioBackend::Impl*>(refCon);
    if (impl == nullptr || impl->cb == nullptr) return noErr;

    const size_t needed = static_cast<size_t>(inNumberFrames) * impl->channels;
    if (impl->scratch.size() < needed) impl->scratch.resize(needed);

    AudioBufferList list{};
    list.mNumberBuffers              = 1;
    list.mBuffers[0].mNumberChannels = impl->channels;
    list.mBuffers[0].mDataByteSize   = static_cast<UInt32>(needed * sizeof(int16_t));
    list.mBuffers[0].mData           = impl->scratch.data();

    const OSStatus r = AudioUnitRender(impl->unit, ioActionFlags, inTimeStamp, inBusNumber,
                                       inNumberFrames, &list);
    if (r != noErr) return r;

    const bool has_host_time =
        inTimeStamp != nullptr && (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid);
    const uint64_t ts_us =
        has_host_time ? MachTimebase::instance().to_us(inTimeStamp->mHostTime) : 0;
    impl->cb(impl->user, impl->scratch.data(), inNumberFrames, ts_us);
    return noErr;
}

} // namespace mockingbird

#endif
