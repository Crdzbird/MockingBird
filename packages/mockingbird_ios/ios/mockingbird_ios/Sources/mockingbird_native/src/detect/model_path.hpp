// Resolves a detector's model path: the config, then an environment
// variable, then a file in the working directory. Paths are UTF-8;
// ort_model_path() turns one into the native form ONNX Runtime takes, which
// is a wide string on Windows.
#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace mockingbird {
inline std::string resolve_model_path(const char* configured, const char* env_var,
                                      const char* fallback) {
    if (configured != nullptr && configured[0] != '\0') return std::string(configured);
    if (const char* env = std::getenv(env_var)) return std::string(env);
    return fallback;
}

inline std::filesystem::path ort_model_path(const std::string& utf8) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

}
