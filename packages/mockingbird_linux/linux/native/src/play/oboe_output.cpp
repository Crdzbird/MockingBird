// Oboe playback backend lifecycle for Android. Audio focus and the
// becoming-noisy broadcast are Java APIs and live in the Flutter plugin.
#include "play/oboe_output.hpp"

#ifdef MOCKINGBIRD_HAVE_OBOE

#include "play/oboe_output_impl.hpp"

namespace mockingbird {
OboeOutput::OboeOutput() : impl_(std::make_shared<Impl>()) {}

OboeOutput::~OboeOutput() {
    std::shared_ptr<oboe::AudioStream> s;
    {
        std::lock_guard lk(impl_->mu);
        impl_->running = false;
        s = std::move(impl_->stream);
    }
    if (s) {
        s->stop();
        s->close();
    }
}

mockingbird_status_t OboeOutput::init(const OutputFormat& fmt, RenderCallback cb, void* user,
                                      OutputNoticeCallback notice, void* notice_user) {
    if (impl_->stream != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cb == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    impl_->fmt         = fmt;
    impl_->cb          = cb;
    impl_->user        = user;
    impl_->notice      = notice;
    impl_->notice_user = notice_user;
    return impl_->open() == oboe::Result::OK ? MOCKINGBIRD_OK : MOCKINGBIRD_ERR_BACKEND;
}

mockingbird_status_t OboeOutput::start() {
    std::lock_guard lk(impl_->mu);
    if (!impl_->stream) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    if (impl_->stream->getState() == oboe::StreamState::Closed && impl_->open() != oboe::Result::OK) {
        return MOCKINGBIRD_ERR_BACKEND;
    }
    if (impl_->stream->requestStart() != oboe::Result::OK) return MOCKINGBIRD_ERR_BACKEND;
    impl_->running = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t OboeOutput::stop() {
    std::shared_ptr<oboe::AudioStream> s;
    {
        std::lock_guard lk(impl_->mu);
        if (!impl_->running) return MOCKINGBIRD_OK;
        impl_->running = false;
        s = impl_->stream;
    }
    if (s) s->stop();
    return MOCKINGBIRD_OK;
}

}

#endif
