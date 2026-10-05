// Diagnostic logging, compiled out unless AURIS_ENABLE_LOG is defined so
// release builds stay silent. Logs control-plane state only, never audio.
#ifndef AURIS_LOG_HPP
#define AURIS_LOG_HPP

#if defined(AURIS_ENABLE_LOG)
#if defined(__ANDROID__)
#include <android/log.h>
#define AURIS_LOG(fmt, ...) \
    __android_log_print(ANDROID_LOG_WARN, "auris", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#include <cstdio>
#define AURIS_LOG(fmt, ...) \
    std::fprintf(stderr, "[auris] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif
#else
#define AURIS_LOG(...) ((void)0)
#endif

#define AURIS_VLOG(...) AURIS_LOG(__VA_ARGS__)

#endif
