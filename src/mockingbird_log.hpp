// Diagnostic logging, compiled out unless MOCKINGBIRD_ENABLE_LOG is defined so
// release builds stay silent. Logs control-plane state only, never audio.
// The disabled form still names its arguments, so nothing reads as unused.
#ifndef MOCKINGBIRD_LOG_HPP
#define MOCKINGBIRD_LOG_HPP

#if defined(MOCKINGBIRD_ENABLE_LOG)
#if defined(__ANDROID__)
#include <android/log.h>
#define MOCKINGBIRD_LOG(fmt, ...) \
    __android_log_print(ANDROID_LOG_WARN, "mockingbird", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#include <cstdio>
#define MOCKINGBIRD_LOG(fmt, ...) \
    std::fprintf(stderr, "[mockingbird] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif
#else
namespace mockingbird {
template <typename... Args>
inline void discard_log(const Args&...) {}
}
#define MOCKINGBIRD_LOG(...) ::mockingbird::discard_log(__VA_ARGS__)
#endif

#define MOCKINGBIRD_VLOG(...) MOCKINGBIRD_LOG(__VA_ARGS__)

#endif
