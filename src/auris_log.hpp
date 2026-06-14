// src/auris_log.hpp
//
// Internal diagnostic logging. Compiled OUT entirely unless AURIS_ENABLE_LOG is
// defined, so RELEASE builds of the library write NOTHING to stderr / logcat /
// syslog. This keeps production quiet and honors the "no audio bytes are logged
// or persisted" contract — these macros only ever log control-plane diagnostics
// (init status, model paths, backend errors), never PCM, but staying silent by
// default removes any doubt and avoids leaking timing/operational signal.
//
// Hosts that need to diagnose an integration build with AURIS_ENABLE_LOG=1
// (e.g. -DAURIS_ENABLE_LOG via the CMake option, or a GCC_PREPROCESSOR_DEFINITION
// in the pod). The two spellings (AURIS_LOG / AURIS_VLOG) are historical and
// identical; both route to one gated implementation.

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

#else  // !AURIS_ENABLE_LOG — no-op, but still parses/typechecks the arguments.

#define AURIS_LOG(...) ((void)0)

#endif

// Historical alias used by the detector translation units.
#define AURIS_VLOG(...) AURIS_LOG(__VA_ARGS__)

#endif  // AURIS_LOG_HPP
