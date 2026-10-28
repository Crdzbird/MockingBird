// Resolves a detector's model path: the config, then an environment
// variable, then a file in the working directory.
#pragma once

#include <cstdlib>
#include <string>

namespace mockingbird {
inline std::string resolve_model_path(const char* configured, const char* env_var,
                                      const char* fallback) {
    if (configured != nullptr && configured[0] != '\0') return std::string(configured);
    if (const char* env = std::getenv(env_var)) return std::string(env);
    return fallback;
}

}
