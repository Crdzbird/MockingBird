// Core Audio output lifecycle; start and stop take the shared iOS audio
// session for playback.
#include "play/coreaudio_output.hpp"

#if defined(__APPLE__)

#include "platform/apple_audio_session.hpp"
#include "play/coreaudio_output_impl.hpp"

namespace mockingbird {
bool CoreAudioOutput::Impl::start_unit() {
    if (running) return true;
    running = AudioOutputUnitStart(unit) == noErr;
    return running;
}

void CoreAudioOutput::Impl::stop_unit() {
    if (running) AudioOutputUnitStop(unit);
    running = false;
}

CoreAudioOutput::CoreAudioOutput() : impl_(std::make_shared<Impl>()) {}

CoreAudioOutput::~CoreAudioOutput() {
    coreaudio_output_unwatch(*impl_);
    std::lock_guard lk(impl_->mu);
    impl_->alive = false;
    impl_->stop_unit();
    if (impl_->unit != nullptr) {
        AudioUnitUninitialize(impl_->unit);
        AudioComponentInstanceDispose(impl_->unit);
        impl_->unit = nullptr;
    }
    if (impl_->session_held) audio_session_release(SessionUse::Playback);
}

mockingbird_status_t CoreAudioOutput::init(const OutputFormat& fmt, RenderCallback cb, void* user,
                                           OutputNoticeCallback notice, void* notice_user) {
    if (impl_->unit != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cb == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    impl_->cb          = cb;
    impl_->user        = user;
    impl_->notice      = notice;
    impl_->notice_user = notice_user;
    impl_->channels    = fmt.channels;
    AudioComponentInstance unit = coreaudio_output_create_unit();
    if (unit == nullptr) return MOCKINGBIRD_ERR_BACKEND;
    if (coreaudio_output_configure(unit, fmt, impl_.get()) != noErr) {
        AudioComponentInstanceDispose(unit);
        return MOCKINGBIRD_ERR_BACKEND;
    }
    impl_->unit = unit;
    coreaudio_output_watch(impl_);
    return MOCKINGBIRD_OK;
}

mockingbird_status_t CoreAudioOutput::start() {
    std::lock_guard lk(impl_->mu);
    if (impl_->unit == nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    if (!impl_->session_held) {
        const mockingbird_status_t s = audio_session_acquire(SessionUse::Playback, 0);
        if (s != MOCKINGBIRD_OK) return s;
        impl_->session_held = true;
    }
    if (!impl_->start_unit()) return MOCKINGBIRD_ERR_BACKEND;
    impl_->wanted = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t CoreAudioOutput::stop() {
    std::lock_guard lk(impl_->mu);
    impl_->wanted = false;
    impl_->stop_unit();
    if (impl_->session_held) audio_session_release(SessionUse::Playback);
    impl_->session_held = false;
    return MOCKINGBIRD_OK;
}

}

#endif
