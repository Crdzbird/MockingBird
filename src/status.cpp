// Pure entry points: no allocation, no state, never throw.
#include "mockingbird/mockingbird.h"

extern "C" {
MOCKINGBIRD_API const char* mockingbird_status_str(int s) {
    switch (s) {
        case MOCKINGBIRD_OK:                  return "MOCKINGBIRD_OK";
        case MOCKINGBIRD_ERR_INVALID_ARG:     return "MOCKINGBIRD_ERR_INVALID_ARG";
        case MOCKINGBIRD_ERR_INVALID_STATE:   return "MOCKINGBIRD_ERR_INVALID_STATE";
        case MOCKINGBIRD_ERR_NO_MEMORY:       return "MOCKINGBIRD_ERR_NO_MEMORY";
        case MOCKINGBIRD_ERR_BACKEND:         return "MOCKINGBIRD_ERR_BACKEND";
        case MOCKINGBIRD_ERR_NOT_RUNNING:     return "MOCKINGBIRD_ERR_NOT_RUNNING";
        case MOCKINGBIRD_ERR_ALREADY_RUNNING: return "MOCKINGBIRD_ERR_ALREADY_RUNNING";
        case MOCKINGBIRD_ERR_QUEUE_EMPTY:     return "MOCKINGBIRD_ERR_QUEUE_EMPTY";
        case MOCKINGBIRD_ERR_PERMISSION:      return "MOCKINGBIRD_ERR_PERMISSION";
        case MOCKINGBIRD_ERR_UNSUPPORTED:     return "MOCKINGBIRD_ERR_UNSUPPORTED";
        case MOCKINGBIRD_ERR_INTERNAL:        return "MOCKINGBIRD_ERR_INTERNAL";
    }
    return "MOCKINGBIRD_UNKNOWN";
}

MOCKINGBIRD_API const char* mockingbird_version_string(void) { return "0.1.0"; }

}
