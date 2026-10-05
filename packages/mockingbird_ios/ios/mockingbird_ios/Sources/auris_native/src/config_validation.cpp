// Structural checks: enums, layout, queue and duration bounds. Every bound
// caps an allocation the worker would otherwise make from untrusted input.
#include "config_validation.hpp"

#include <cmath>

namespace auris {

namespace {

constexpr uint32_t kMaxPcmRingFrames       = 1u << 24;
constexpr uint32_t kMaxPacketQueueCapacity = 1u << 16;
constexpr uint32_t kMaxDurationMs          = 300000u;

bool valid_sample_rate(uint32_t hz) {
    return hz == 8000 || hz == 16000 || hz == 32000 || hz == 48000;
}

bool valid_frame_ms(uint32_t ms) { return ms == 10 || ms == 20 || ms == 30; }

bool valid_codec(int c) {
    return c == AURIS_CODEC_PCM_S16LE || c == AURIS_CODEC_OPUS || c == AURIS_CODEC_FLAC;
}

bool valid_backend(int b) {
    return b == AURIS_BACKEND_AUTO || b == AURIS_BACKEND_MINIAUDIO ||
           b == AURIS_BACKEND_OBOE || b == AURIS_BACKEND_COREAUDIO ||
           b == AURIS_BACKEND_HOST_PCM;
}

bool valid_vad_tier(int t) { return t == AURIS_VAD_FAST || t == AURIS_VAD_NEURAL; }

bool valid_queues(const auris_config_t& c) {
    return c.pcm_ring_frames != 0 && c.pcm_ring_frames <= kMaxPcmRingFrames &&
           c.packet_queue_capacity != 0 &&
           c.packet_queue_capacity <= kMaxPacketQueueCapacity &&
           c.pcm_tap_frames <= kMaxPcmRingFrames;
}

bool valid_performance(const auris_config_t& c) {
    return std::isfinite(c.silence_gate_dbfs) && c.silence_gate_dbfs <= 0.0f &&
           c.degrade_busy_ppm <= 1000000u && c.worker_batch_frames <= 64u;
}

bool valid_durations(const auris_config_t& c) {
    return c.pre_roll_ms <= kMaxDurationMs && c.post_roll_ms <= kMaxDurationMs &&
           c.vad_hangover_ms <= kMaxDurationMs &&
           c.anomaly_background_ms <= kMaxDurationMs &&
           c.segment_max_ms <= kMaxDurationMs &&
           c.sound_class_interval_ms <= kMaxDurationMs;
}

} // namespace

auris_status_t validate_config(const auris_config_t& c) {
    const int backend       = enum_as_int(c.backend);
    const int sample_format = enum_as_int(c.sample_format);
    const int vad_tier      = enum_as_int(c.vad_tier);
    const int codec         = enum_as_int(c.codec);

    if (!valid_backend(backend))              return AURIS_ERR_INVALID_ARG;
    if (!valid_sample_rate(c.sample_rate_hz)) return AURIS_ERR_INVALID_ARG;
    if (c.channels != kChannelsV1)            return AURIS_ERR_INVALID_ARG;
    if (sample_format != AURIS_SAMPLE_S16)    return AURIS_ERR_INVALID_ARG;
    if (!valid_frame_ms(c.frame_ms))          return AURIS_ERR_INVALID_ARG;
    if (!valid_codec(codec))                  return AURIS_ERR_INVALID_ARG;
    if (!valid_vad_tier(vad_tier))            return AURIS_ERR_INVALID_ARG;
    if (!valid_queues(c))                     return AURIS_ERR_INVALID_ARG;
    if (!valid_performance(c))                return AURIS_ERR_INVALID_ARG;
    if (!valid_durations(c))                  return AURIS_ERR_INVALID_ARG;

    const auris_status_t ranges = check_detector_ranges(c, vad_tier, codec);
    if (ranges != AURIS_OK) return ranges;
    return check_build_support(c, vad_tier, codec);
}

uint32_t samples_per_frame(const auris_config_t& c) {
    return (c.sample_rate_hz / 1000u) * c.frame_ms;
}

size_t batch_samples_for(const auris_config_t& c, uint32_t batch_frames) {
    if (batch_frames <= 1u) return 0;
    const size_t frame  = static_cast<size_t>(c.sample_rate_hz / 1000u) * c.frame_ms;
    size_t       wanted = static_cast<size_t>(batch_frames) * frame;
    const size_t cap    = static_cast<size_t>(c.pcm_ring_frames) / 2u;
    if (wanted > cap) wanted = cap;
    return wanted < 2u * frame ? 0 : wanted;
}

} // namespace auris
