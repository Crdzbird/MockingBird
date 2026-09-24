// src/detect/ort_env.hpp
//
// A single process-wide Ort::Env shared by every ORT-backed detector.
// ONNX Runtime documents Env as a process singleton — it owns the thread pools
// and the logging manager — so one Env per component (neural VAD + YAMNet) just
// duplicates those. The function-local static is created on first use and lives
// until process exit, which outlives every Ort::Session created against it.

#pragma once

#if __has_include(<onnxruntime_cxx_api.h>)
  #include <onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime-c/onnxruntime_cxx_api.h>)
  #include <onnxruntime-c/onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime/onnxruntime_cxx_api.h>)
  // Framework-style: Microsoft's ONNX Runtime Swift package vends an
  // xcframework, so headers live in onnxruntime.framework/Headers.
  #include <onnxruntime/onnxruntime_cxx_api.h>
#else
  #error "ONNX Runtime header not found for ort_env"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace auris {

// Execution-provider selection for measurement: AURIS_ORT_EP=coreml appends
// the CoreML EP through ORT's provider-options API (the legacy
// OrtSessionOptionsAppendExecutionProvider_CoreML factory is deprecated and
// terminates the process on newer macOS). Kept env-driven until a measured
// win justifies a config field. No-op when the provider is not compiled in.
inline void apply_execution_provider(Ort::SessionOptions& opts) {
    const char* ep = std::getenv("AURIS_ORT_EP");
    if (ep == nullptr || std::strcmp(ep, "coreml") != 0) return;
    try {
        std::unordered_map<std::string, std::string> o{
            {"ModelFormat", "MLProgram"},
            {"MLComputeUnits", "ALL"},
            {"EnableOnSubgraphs", "1"},
        };
        opts.AppendExecutionProvider("CoreML", o);
    } catch (const Ort::Exception& ex) {
        std::fprintf(stderr, "auris: CoreML EP unavailable: %s\n", ex.what());
    }
}

inline Ort::Env& shared_ort_env() {
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "auris");
    return env;
}

} // namespace auris
