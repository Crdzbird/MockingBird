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
#else
  #error "ONNX Runtime header not found for ort_env"
#endif

namespace auris {

inline Ort::Env& shared_ort_env() {
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "auris");
    return env;
}

} // namespace auris
