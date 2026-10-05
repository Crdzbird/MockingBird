// Pure entry points: no allocation, no state, never throw.
#include "auris/auris.h"

extern "C" {

AURIS_API const char* auris_status_str(int s) {
    switch (s) {
        case AURIS_OK:                  return "AURIS_OK";
        case AURIS_ERR_INVALID_ARG:     return "AURIS_ERR_INVALID_ARG";
        case AURIS_ERR_INVALID_STATE:   return "AURIS_ERR_INVALID_STATE";
        case AURIS_ERR_NO_MEMORY:       return "AURIS_ERR_NO_MEMORY";
        case AURIS_ERR_BACKEND:         return "AURIS_ERR_BACKEND";
        case AURIS_ERR_NOT_RUNNING:     return "AURIS_ERR_NOT_RUNNING";
        case AURIS_ERR_ALREADY_RUNNING: return "AURIS_ERR_ALREADY_RUNNING";
        case AURIS_ERR_QUEUE_EMPTY:     return "AURIS_ERR_QUEUE_EMPTY";
        case AURIS_ERR_PERMISSION:      return "AURIS_ERR_PERMISSION";
        case AURIS_ERR_UNSUPPORTED:     return "AURIS_ERR_UNSUPPORTED";
        case AURIS_ERR_INTERNAL:        return "AURIS_ERR_INTERNAL";
    }
    return "AURIS_UNKNOWN";
}

AURIS_API const char* auris_version_string(void) { return "0.1.0"; }

} // extern "C"
