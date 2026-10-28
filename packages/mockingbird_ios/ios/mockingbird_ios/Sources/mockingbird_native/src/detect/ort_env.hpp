// One process-wide Ort::Env, as ONNX Runtime documents, shared by every
// session; and the optional execution provider used for measurements, picked
// by MOCKINGBIRD_ORT_EP: coreml, xnnpack, or nnapi on Android.
//
// The official ONNX Runtime 1.30 builds for Android, iOS, macOS and Linux
// carry Microsoft's 1DS telemetry client, which uploads usage events and a
// persistent device id unless the process opts out before the first Ort::Env
// exists. The engine opts out: it sets ORT_DISABLE_TELEMETRY=1 when the host
// has not set the variable itself, then disables telemetry events on the
// environment, which also covers Windows' ETW provider.
#pragma once

#include "detect/ort_headers.hpp"

#if defined(__ANDROID__) && __has_include(<nnapi_provider_factory.h>)
#include <nnapi_provider_factory.h>
#define MOCKINGBIRD_HAVE_NNAPI_EP 1
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace mockingbird {
inline void apply_execution_provider(Ort::SessionOptions& opts) {
    const char* ep = std::getenv("MOCKINGBIRD_ORT_EP");
    if (ep == nullptr) return;
    try {
        if (std::strcmp(ep, "xnnpack") == 0) {
            opts.AppendExecutionProvider("XNNPACK", {{"intra_op_num_threads", "1"}});
            return;
        }
#ifdef MOCKINGBIRD_HAVE_NNAPI_EP
        if (std::strcmp(ep, "nnapi") == 0) {
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_Nnapi(opts, 0));
            return;
        }
#endif
        if (std::strcmp(ep, "coreml") != 0) return;
        std::unordered_map<std::string, std::string> o{
            {"ModelFormat", "MLProgram"},
            {"MLComputeUnits", "ALL"},
            {"EnableOnSubgraphs", "1"},
        };
        opts.AppendExecutionProvider("CoreML", o);
    } catch (const Ort::Exception& ex) {
        std::fprintf(stderr, "mockingbird: %s EP unavailable: %s\n", ep, ex.what());
    }
}

inline void opt_out_of_ort_telemetry() {
    if (std::getenv("ORT_DISABLE_TELEMETRY") != nullptr) return;
#if defined(_WIN32)
    _putenv_s("ORT_DISABLE_TELEMETRY", "1");
#else
    setenv("ORT_DISABLE_TELEMETRY", "1", 0);
#endif
}

inline Ort::Env& shared_ort_env() {
    static Ort::Env env = [] {
        opt_out_of_ort_telemetry();
        Ort::Env created(ORT_LOGGING_LEVEL_WARNING, "mockingbird");
        created.DisableTelemetryEvents();
        return created;
    }();
    return env;
}

}
