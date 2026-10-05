// One process-wide Ort::Env, as ONNX Runtime documents, shared by every
// session; and the optional execution provider used for measurements.
#pragma once

#include "detect/ort_headers.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace auris {

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
