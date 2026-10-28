// mockingbird_list_input_devices and mockingbird_set_input_device. Names and
// ids longer than the record's fields are cut at a UTF-8 character boundary.
#include "mockingbird/mockingbird.h"
#include "capture/capture_backend.hpp"
#include "engine.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr size_t kField = 256;

void copy_utf8(char* dst, const std::string& src) {
    size_t n = src.size() < kField - 1 ? src.size() : kField - 1;
    while (n > 0 && n < src.size() && (static_cast<unsigned char>(src[n]) & 0xC0u) == 0x80u) --n;
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

}

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_list_input_devices(mockingbird_backend_t backend,
                                                  mockingbird_device_t* out, uint32_t capacity,
                                                  uint32_t* out_count) {
    if (out_count == nullptr || (capacity > 0 && out == nullptr)) return MOCKINGBIRD_ERR_INVALID_ARG;
    *out_count = 0;
    try {
        std::vector<mockingbird::InputDevice> devices;
        const mockingbird_status_t s = mockingbird::list_input_devices(backend, devices);
        if (s != MOCKINGBIRD_OK) return s;
        *out_count = static_cast<uint32_t>(devices.size());
        for (uint32_t i = 0; i < capacity && i < devices.size(); ++i) {
            mockingbird_device_t& d = out[i];
            std::memset(&d, 0, sizeof d);
            copy_utf8(d.id, devices[i].id);
            copy_utf8(d.name, devices[i].name);
            d.is_default = devices[i].is_default ? 1 : 0;
        }
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_set_input_device(mockingbird_engine_t* e,
                                                const char* device_id) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (device_id != nullptr && std::strlen(device_id) >= kField) return MOCKINGBIRD_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->lifecycle_mu);
        if (e->cfg.backend == MOCKINGBIRD_BACKEND_HOST_PCM) return MOCKINGBIRD_ERR_UNSUPPORTED;
        if (e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_INVALID_STATE;
        e->input_device_id = device_id == nullptr ? std::string() : std::string(device_id);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

}
