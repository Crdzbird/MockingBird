// Messages for a host runtime's post function (mockingbird_post_cb). The
// layout mirrors the head of Dart's Dart_CObject: a 32-bit type, then a
// union whose alignment is that of int64_t, double and a pointer, padded to
// cover the largest member on every ABI. Only null and int64 are sent.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>

namespace mockingbird {
struct HostMessage {
    int32_t type = 0;
    union {
        int64_t as_int64;
        double  as_double;
        void*   as_pointer[5];
    } value{};
};

inline constexpr int32_t kHostMessageNull  = 0;
inline constexpr int32_t kHostMessageInt64 = 3;

inline void post_null(mockingbird_post_cb post, int64_t port) noexcept {
    if (post == nullptr) return;
    HostMessage m{};
    m.type = kHostMessageNull;
    post(port, &m);
}

inline void post_done(mockingbird_post_cb post, int64_t port, uint32_t op,
                      int32_t status) noexcept {
    if (post == nullptr) return;
    HostMessage m{};
    m.type           = kHostMessageInt64;
    m.value.as_int64 = static_cast<int64_t>((static_cast<uint64_t>(op) << 32) |
                                            static_cast<uint32_t>(status));
    post(port, &m);
}

}
