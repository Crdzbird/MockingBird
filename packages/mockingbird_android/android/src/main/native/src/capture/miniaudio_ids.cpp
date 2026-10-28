// String-named backends (WASAPI, ALSA, PulseAudio, Core Audio and the like)
// keep their own names as ids; the rest are written as hex of the id bytes.
#include "capture/miniaudio_ids.hpp"

#include <cstddef>
#include <cstring>

namespace mockingbird {
namespace {
std::string hex(const void* p, size_t n) {
    static const char* digits = "0123456789abcdef";
    const auto* b = static_cast<const unsigned char*>(p);
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += digits[b[i] >> 4];
        s += digits[b[i] & 0xF];
    }
    return s;
}

std::string bounded(const char* s, size_t cap) {
    return std::string(s, strnlen(s, cap));
}

}

std::string miniaudio_encode_id(ma_backend backend, const ma_device_id& id) {
    switch (backend) {
        case ma_backend_wasapi: {
            std::string s;
            for (size_t i = 0; i < sizeof(id.wasapi) / sizeof(id.wasapi[0]) && id.wasapi[i] != 0; ++i) {
                const auto c = static_cast<unsigned>(id.wasapi[i]);
                s += c < 0x80u ? static_cast<char>(c) : '?';
            }
            return s;
        }
        case ma_backend_alsa:      return bounded(id.alsa, sizeof id.alsa);
        case ma_backend_pulseaudio: return bounded(id.pulse, sizeof id.pulse);
        case ma_backend_coreaudio: return bounded(id.coreaudio, sizeof id.coreaudio);
        case ma_backend_sndio:     return bounded(id.sndio, sizeof id.sndio);
        case ma_backend_audio4:    return bounded(id.audio4, sizeof id.audio4);
        case ma_backend_oss:       return bounded(id.oss, sizeof id.oss);
        case ma_backend_dsound:    return hex(id.dsound, sizeof id.dsound);
        case ma_backend_winmm:     return hex(&id.winmm, sizeof id.winmm);
        case ma_backend_aaudio:    return hex(&id.aaudio, sizeof id.aaudio);
        case ma_backend_opensl:    return hex(&id.opensl, sizeof id.opensl);
        default:                   return hex(&id.nullbackend, sizeof id.nullbackend);
    }
}

bool miniaudio_find_capture_device(ma_context& ctx, const std::string& id, ma_device_id& out) {
    ma_device_info* playback = nullptr;
    ma_device_info* capture  = nullptr;
    ma_uint32 n_playback = 0;
    ma_uint32 n_capture  = 0;
    if (ma_context_get_devices(&ctx, &playback, &n_playback, &capture, &n_capture) != MA_SUCCESS) {
        return false;
    }
    for (ma_uint32 i = 0; i < n_capture; ++i) {
        if (miniaudio_encode_id(ctx.backend, capture[i].id) == id) {
            out = capture[i].id;
            return true;
        }
    }
    return false;
}

}
