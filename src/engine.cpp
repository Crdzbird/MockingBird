// src/engine.cpp
//
// C ABI implementation. // ABI-SAFE: no C++ exception escapes any entry point.

#include "auris/auris.h"
#include "capture/capture_backend.hpp"
#include "detect/detector.hpp"
#include "detect/sound_classifier.hpp"
#include "encode/encoder.hpp"
#include "engine.hpp"
#include "preprocess/preprocessor.hpp"
#include "preroll_ring.hpp"

#include "auris_log.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

// ============================================================================
// Internal helpers (unnamed namespace).
// ============================================================================

namespace {

constexpr uint16_t kChannelsV1 = 1;

bool is_valid_sample_rate(uint32_t hz) {
    // // VERIFY  libfvad documented set; libopus accepts a similar set.
    return hz == 8000 || hz == 16000 || hz == 32000 || hz == 48000;
}

bool is_valid_frame_ms(uint32_t ms) {
    // // VERIFY  libfvad documented set.
    return ms == 10 || ms == 20 || ms == 30;
}

// Read a config enum field as its raw integer WITHOUT an enum lvalue-to-rvalue
// conversion. A host (or fuzzer) may place ANY int-sized value in these fields,
// and loading an out-of-range value *through the enum type* is undefined
// behavior; reading the underlying bytes is well-defined. We validate the int
// against the known enumerators before anything ever loads it as an enum.
template <typename E>
int enum_as_int(const E& field) {
    static_assert(sizeof(E) == sizeof(int), "ABI enum must be int-sized");
    int v;
    std::memcpy(&v, &field, sizeof(int));
    return v;
}

bool is_valid_codec(int c) {
    return c == AURIS_CODEC_PCM_S16LE
        || c == AURIS_CODEC_OPUS
        || c == AURIS_CODEC_FLAC;
}

bool is_valid_backend(int b) {
    return b == AURIS_BACKEND_AUTO
        || b == AURIS_BACKEND_MINIAUDIO
        || b == AURIS_BACKEND_OBOE
        || b == AURIS_BACKEND_COREAUDIO
        || b == AURIS_BACKEND_HOST_PCM;
}

bool is_valid_vad_tier(int t) {
    return t == AURIS_VAD_FAST || t == AURIS_VAD_NEURAL;
}

auris_status_t validate_config(const auris_config_t& c) {
    // Enum fields come from untrusted input — read them as raw ints first.
    const int backend       = enum_as_int(c.backend);
    const int sample_format = enum_as_int(c.sample_format);
    const int vad_tier      = enum_as_int(c.vad_tier);
    const int codec         = enum_as_int(c.codec);

    if (!is_valid_backend(backend))              return AURIS_ERR_INVALID_ARG;
    if (!is_valid_sample_rate(c.sample_rate_hz)) return AURIS_ERR_INVALID_ARG;
    if (c.channels != kChannelsV1)               return AURIS_ERR_INVALID_ARG;
    if (sample_format != AURIS_SAMPLE_S16)       return AURIS_ERR_INVALID_ARG;
    if (!is_valid_frame_ms(c.frame_ms))          return AURIS_ERR_INVALID_ARG;
    if (!is_valid_codec(codec))                  return AURIS_ERR_INVALID_ARG;
    if (!is_valid_vad_tier(vad_tier))            return AURIS_ERR_INVALID_ARG;
    // Queue sizes must be non-zero and sanely bounded. Without an upper bound,
    // a hostile/buggy host can request a multi-GB ring; on over-committing
    // platforms (macOS) auris_create even "succeeds", then commits the pages
    // later and OOMs the machine. Reject absurd sizes up front. The caps are
    // already extravagant: 16 M frames ≈ 1000 s of 16 kHz mono PCM, and 64 K
    // in-flight packets.
    constexpr uint32_t kMaxPcmRingFrames      = 1u << 24;   // 16,777,216
    constexpr uint32_t kMaxPacketQueueCapacity = 1u << 16;  // 65,536
    if (c.pcm_ring_frames == 0 ||
        c.pcm_ring_frames > kMaxPcmRingFrames)   return AURIS_ERR_INVALID_ARG;
    if (c.packet_queue_capacity == 0 ||
        c.packet_queue_capacity > kMaxPacketQueueCapacity) return AURIS_ERR_INVALID_ARG;

    // Duration fields (ms) become frame counts and, for pre-roll and the anomaly
    // background window, buffer allocations on the worker thread. Without a bound
    // a value like UINT32_MAX allocates gigabytes and throws bad_alloc on the
    // worker (which cannot surface it through a status code). Cap generously:
    // 300 s bounds every derived allocation to tens of MB.
    constexpr uint32_t kMaxDurationMs = 300000u;   // 5 minutes
    if (c.pre_roll_ms             > kMaxDurationMs ||
        c.post_roll_ms            > kMaxDurationMs ||
        c.vad_hangover_ms         > kMaxDurationMs ||
        c.anomaly_background_ms   > kMaxDurationMs ||
        c.segment_max_ms          > kMaxDurationMs ||
        c.sound_class_interval_ms > kMaxDurationMs) {
        return AURIS_ERR_INVALID_ARG;
    }

    // Float fields must be finite; a NaN makes every threshold comparison in a
    // detector false, and -inf makes an anomaly fire on every frame — a silent
    // detection-integrity failure. Range-check only the fields whose detector is
    // active so a garbage-but-unused field never rejects a valid config.
    if (c.enable_vad && vad_tier == AURIS_VAD_NEURAL) {
        if (!std::isfinite(c.vad_neural_threshold) ||
            c.vad_neural_threshold < 0.0f || c.vad_neural_threshold > 1.0f) {
            return AURIS_ERR_INVALID_ARG;
        }
    }
    if (c.enable_anomaly) {
        if (!std::isfinite(c.anomaly_rms_z)  || c.anomaly_rms_z  < 0.0f ||
            !std::isfinite(c.anomaly_flux_z) || c.anomaly_flux_z < 0.0f) {
            return AURIS_ERR_INVALID_ARG;
        }
    }
    if (c.enable_sound_class) {
        if (!std::isfinite(c.sound_class_threshold) ||
            c.sound_class_threshold < 0.0f || c.sound_class_threshold > 1.0f) {
            return AURIS_ERR_INVALID_ARG;
        }
    }

    if (vad_tier == AURIS_VAD_FAST) {
        if (c.vad_aggressiveness < 0 || c.vad_aggressiveness > 3) {
            return AURIS_ERR_INVALID_ARG;
        }
    }
    if (codec == AURIS_CODEC_OPUS) {
        // // VERIFY  Tighten against vendored libopus version.
        if (c.opus_bitrate_bps < 6000u || c.opus_bitrate_bps > 510000u) {
            return AURIS_ERR_INVALID_ARG;
        }
        if (c.opus_complexity < 0 || c.opus_complexity > 10) {
            return AURIS_ERR_INVALID_ARG;
        }
    }
#ifndef AURIS_HAVE_NEURAL_VAD
    if (vad_tier == AURIS_VAD_NEURAL) return AURIS_ERR_UNSUPPORTED;
#endif
#ifndef AURIS_HAVE_RNNOISE
    if (c.enable_rnnoise) return AURIS_ERR_UNSUPPORTED;
#else
    // RNNoise bridges through 48 kHz, so it needs an integer rate ratio.
    // {8000, 16000, 48000} qualify; 32000 does not.
    if (c.enable_rnnoise && (c.sample_rate_hz == 0 ||
                             (48000u % c.sample_rate_hz) != 0)) {
        return AURIS_ERR_INVALID_ARG;
    }
#endif
#ifndef AURIS_HAVE_OPUS
    if (codec == AURIS_CODEC_OPUS) return AURIS_ERR_UNSUPPORTED;
#endif
#ifndef AURIS_HAVE_FLAC
    if (codec == AURIS_CODEC_FLAC) return AURIS_ERR_UNSUPPORTED;
#endif
    return AURIS_OK;
}

uint32_t samples_per_frame(const auris_config_t& c) {
    // Mono v1: samples == frames.
    return (c.sample_rate_hz / 1000u) * c.frame_ms;
}

uint64_t monotonic_now_us() {
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            clock::now().time_since_epoch()).count());
}

// Single source of truth for "PCM arrived from some producer". Both
// auris_push_pcm (HOST_PCM mode, called on the host thread) and the live
// capture backend's data callback (called on T1) route through this.
//
// // PERF-CRITICAL  Lock-free, alloc-free. Two atomic loads, a memcpy via
// SpscRing::push, a few atomic increments, and one wake notify.
void engine_ingest_pcm(auris_engine* e,
                       const int16_t* samples,
                       uint32_t       frame_count,
                       uint64_t       capture_ts_us) noexcept {
    e->last_capture_ts_us.store(capture_ts_us, std::memory_order_release);
    // Record the first frame's timestamp once; the worker derives every per-frame
    // timestamp from it deterministically (independent of producer/worker timing).
    uint64_t expected_first = ~uint64_t{0};
    e->first_capture_ts_us.compare_exchange_strong(
        expected_first, capture_ts_us, std::memory_order_relaxed);

    const size_t written = e->pcm_ring.push(samples, static_cast<size_t>(frame_count));
    e->frames_captured.fetch_add(written, std::memory_order_relaxed);
    if (written < frame_count) {
        e->frames_dropped_capture.fetch_add(frame_count - written,
                                            std::memory_order_relaxed);
    }

    e->wake_count.fetch_add(1, std::memory_order_release);
    e->wake_count.notify_one();
}

// Adapter: capture backend's plain function-pointer callback shape.
void backend_pcm_callback(void*          user,
                          const int16_t* samples,
                          uint32_t       frame_count,
                          uint64_t       capture_ts_us) noexcept {
    engine_ingest_pcm(static_cast<auris_engine*>(user),
                      samples, frame_count, capture_ts_us);
}

// Hard cap on the pull-mode event queue. A push-only host (NativeCallable
// listener, JNI bridge) never drains the pull queue, so without a cap it
// would grow without bound. The cap drops OLDEST so the most recent events
// survive — matches the packet queue semantics (§4).
constexpr size_t kPullEventQueueCap = 1024;

void emit_event(auris_engine* e, const auris_event_t& evt) {
    // Push into pull-mode queue, bounded with drop-oldest.
    {
        std::lock_guard lk(e->evt_mu);
        while (e->evt_queue.size() >= kPullEventQueueCap) {
            e->evt_queue.pop_front();
        }
        e->evt_queue.push_back(evt);
    }
    // Fire push callback under a snapshot. The callback must be non-blocking
    // per the ABI contract; we still guard against accidental exceptions.
    auris_event_cb cb;
    void*          user;
    {
        std::lock_guard lk(e->cb_mu);
        cb   = e->event_cb;
        user = e->event_user;
    }
    if (cb != nullptr) {
        try {
            cb(&evt, user);
        } catch (...) {
            // // ABI-SAFE  swallow; misbehaving host must not corrupt engine.
        }
    }
}

// ----------------------------------------------------------------------------
// Worker loop (T2).
//
// Step-3 pipeline (VAD-driven): each frame popped from the ring is fed to a
// VAD detector. Segments open on the rising edge (silence -> speech), close
// on the falling edge after `vad_hangover_ms` of continuous silence. Packets
// only flow while a segment is open, so the host receives the actual speech,
// not the silence between phrases. Pre-roll (retaining frames before the
// trigger) lands in step 4 alongside Opus encoding.
//
// Anomaly gate (PFFFT-based) wires in next; structurally it fans into the
// same open_segment / emit_packet path with a different trigger and an
// ANOMALY event type.
// ----------------------------------------------------------------------------

void worker_run(auris_engine* e) {
    const uint32_t           frame_samples = samples_per_frame(e->cfg);
    std::vector<int16_t>     scratch(frame_samples);
    uint64_t                 last_seen_wake     = 0;
    uint64_t                 open_segment_id    = 0;   // 0 == no segment open
    uint64_t                 segment_start_us   = 0;
    uint32_t                 segment_pkt_index  = 0;

    // Deterministic per-frame timestamp clock: now_ts(frame) = base + index*frame_us,
    // where base is the first pushed frame's capture timestamp. This makes packet
    // and event timestamps a pure function of the input (frame position), not of
    // when the worker happened to run relative to the producer.
    const uint64_t           frame_us           = static_cast<uint64_t>(e->cfg.frame_ms) * 1000ull;
    uint64_t                 frame_index        = 0;

    // Detector state. VAD and anomaly run in parallel per frame; either
    // can open a segment. When both are disabled the worker degrades to the
    // step-1 passthrough shape (one always-on segment, every frame emitted,
    // no SPEECH_* / ANOMALY events).
    auto init_detector = [&](std::unique_ptr<auris::Detector> d,
                             [[maybe_unused]] const char* tag) {
        if (!d) {
            AURIS_LOG("detector '%s' factory returned null "
                      "(feature not compiled in?)", tag);
            return std::pair{std::move(d), false};
        }
        const auris_status_t s = d->init(e->cfg);
        if (s == AURIS_OK) {
            AURIS_LOG("detector '%s' init OK", tag);
            return std::pair{std::move(d), true};
        }
        AURIS_LOG("detector '%s' init FAILED: %d", tag, static_cast<int>(s));
        // // ABI-SAFE  Surface the init failure as a backend error so the
        // host knows; the worker keeps running with that detector disabled.
        auris_event_t evt{};
        evt.type         = AURIS_EVT_BACKEND_ERROR;
        evt.reason_mask  = static_cast<uint32_t>(s);
        evt.timestamp_us = e->last_capture_ts_us.load(std::memory_order_relaxed);
        emit_event(e, evt);
        return std::pair{std::move(d), false};
    };

    // Optional RNNoise preprocessing. Runs first on each frame so the
    // detectors AND the encoded output both see the cleaned signal. On init
    // failure we surface a backend error and fall back to passthrough.
    std::unique_ptr<auris::Preprocessor> preproc;
    bool preproc_ready = false;
    if (e->cfg.enable_rnnoise) {
        preproc = auris::make_rnnoise_preprocessor();
        if (!preproc) {
            AURIS_LOG("rnnoise enabled but not compiled in "
                      "(AURIS_HAVE_RNNOISE off)");
        } else {
            const auris_status_t s = preproc->init(e->cfg);
            preproc_ready = (s == AURIS_OK);
            AURIS_LOG("preprocess 'rnnoise' init %s (%d)",
                      preproc_ready ? "OK" : "FAILED", static_cast<int>(s));
            if (!preproc_ready) {
                auris_event_t evt{};
                evt.type         = AURIS_EVT_BACKEND_ERROR;
                evt.reason_mask  = static_cast<uint32_t>(s);
                evt.timestamp_us = e->last_capture_ts_us.load(std::memory_order_relaxed);
                emit_event(e, evt);
                preproc.reset();
            }
        }
    }

    std::unique_ptr<auris::Detector> vad;
    bool vad_ready = false;
    if (e->cfg.enable_vad) {
        auto pr = init_detector(auris::make_vad(e->cfg.vad_tier),
                                e->cfg.vad_tier == AURIS_VAD_NEURAL
                                    ? "neural-vad" : "fast-vad");
        vad = std::move(pr.first);
        vad_ready = pr.second;
    }
    std::unique_ptr<auris::Detector> anomaly;
    bool anomaly_ready = false;
    if (e->cfg.enable_anomaly) {
        auto pr = init_detector(auris::make_anomaly(), "anomaly");
        anomaly = std::move(pr.first);
        anomaly_ready = pr.second;
    }

    // YAMNet sound classifier (semantic labeling). Runs alongside VAD +
    // anomaly; its findings DO open segments but do not change the
    // VAD/anomaly hangover bookkeeping. On init failure we surface a
    // backend error and skip the classifier path.
    std::unique_ptr<auris::SoundClassifier> classifier;
    bool classifier_ready = false;
    if (e->cfg.enable_sound_class) {
        AURIS_LOG("sound_class enabled, model_path='%s'",
                  e->cfg.sound_class_model_path ? e->cfg.sound_class_model_path
                                                : "(null)");
        classifier = auris::make_sound_classifier();
        if (!classifier) {
            AURIS_LOG("make_sound_classifier returned null "
                      "(AURIS_HAVE_SOUND_CLASS not compiled in?)");
        } else {
            const auris_status_t s = classifier->init(e->cfg);
            classifier_ready = (s == AURIS_OK);
            AURIS_LOG("sound_class init %s (%d)",
                      classifier_ready ? "OK" : "FAILED", static_cast<int>(s));
            if (!classifier_ready) {
                auris_event_t evt{};
                evt.type         = AURIS_EVT_BACKEND_ERROR;
                evt.reason_mask  = static_cast<uint32_t>(s);
                evt.timestamp_us = e->last_capture_ts_us.load(std::memory_order_relaxed);
                emit_event(e, evt);
                classifier.reset();
            }
        }
    }
    // --- live-tunable state (see auris_update_config) -----------------------
    // These mirror the tunable config subset. They start from the create-time
    // config and are refreshed from e->live_cfg whenever the host bumps
    // tune_generation. Everything else in e->cfg is structural and immutable.
    auto threshold_or = [](float v, float fallback) {
        return (v > 0.0f && v < 1.0f) ? v : fallback;
    };
    auto hangover_of = [&](uint32_t ms) -> uint32_t {
        return (ms == 0) ? 0u : (ms + e->cfg.frame_ms - 1u) / e->cfg.frame_ms;
    };

    float    classifier_threshold = threshold_or(e->cfg.sound_class_threshold, 0.4f);
    uint32_t hangover_frames      = hangover_of(e->cfg.vad_hangover_ms);
    bool     vad_enabled          = e->cfg.enable_vad         != 0;
    bool     anomaly_enabled      = e->cfg.enable_anomaly     != 0;
    bool     sound_class_enabled  = e->cfg.enable_sound_class != 0;
    uint64_t seen_tune_generation = e->tune_generation.load(std::memory_order_acquire);

    // Pull a new tunable snapshot if the host retuned. One relaxed atomic load
    // per frame in the common (unchanged) case; the mutex is taken only on an
    // actual change, and never while a detector is mid-frame.
    auto refresh_tunables = [&]() {
        const uint64_t gen = e->tune_generation.load(std::memory_order_acquire);
        if (gen == seen_tune_generation) return;
        seen_tune_generation = gen;
        auris_config_t live;
        {
            std::lock_guard lk(e->tune_mu);
            live = e->live_cfg;
        }
        vad_enabled         = live.enable_vad         != 0;
        anomaly_enabled     = live.enable_anomaly     != 0;
        sound_class_enabled = live.enable_sound_class != 0;
        classifier_threshold = threshold_or(live.sound_class_threshold, 0.4f);
        hangover_frames      = hangover_of(live.vad_hangover_ms);
        if (vad)     vad->retune(live);
        if (anomaly) anomaly->retune(live);
        AURIS_VLOG("retuned to generation %llu",
                   static_cast<unsigned long long>(gen));
    };

    bool     segment_open          = false;
    bool     segment_had_speech    = false;
    bool     vad_was_active        = false;
    bool     ano_was_active        = false;
    uint32_t silence_frames        = 0;

    auto open_segment = [&](uint64_t at_us, float confidence) {
        if (open_segment_id != 0) return;
        open_segment_id   = e->next_segment_id++;
        segment_start_us  = at_us;
        segment_pkt_index = 0;
        e->segments_opened.fetch_add(1, std::memory_order_relaxed);

        auris_event_t evt{};
        evt.type         = AURIS_EVT_SEGMENT_BEGIN;
        evt.segment_id   = open_segment_id;
        evt.timestamp_us = at_us;
        evt.confidence   = confidence;
        emit_event(e, evt);
    };

    auto close_segment = [&](uint64_t at_us) {
        if (open_segment_id == 0) return;
        auris_event_t evt{};
        evt.type         = AURIS_EVT_SEGMENT_END;
        evt.segment_id   = open_segment_id;
        evt.timestamp_us = at_us;
        emit_event(e, evt);
        e->segments_closed.fetch_add(1, std::memory_order_relaxed);
        open_segment_id = 0;
    };

    auto emit_packet = [&](const int16_t* samples,
                           uint64_t       now_ts,
                           uint32_t       extra_flags) {
        auto* slot = e->packets.acquire();
        if (slot == nullptr) {
            e->packets_dropped.fetch_add(1, std::memory_order_relaxed);
            auris_event_t evt{};
            evt.type         = AURIS_EVT_OVERRUN;
            evt.timestamp_us = now_ts;
            emit_event(e, evt);
            return;
        }

        // Encode into the slot's storage.
        const int n = e->encoder->encode(samples,
                                         frame_samples,
                                         slot->buf.get(),
                                         static_cast<uint32_t>(e->packets.max_payload_bytes()));
        if (n <= 0) {
            // Encoder failure or DTX. Recycle the slot without committing.
            slot->size = 0;
            e->packets.release_unused(slot);
            if (n < 0) e->packets_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        slot->codec         = e->encoder->codec();
        slot->segment_id    = open_segment_id;
        slot->capture_ts_us = now_ts;
        slot->pts_us        = (now_ts >= segment_start_us)
                                  ? (now_ts - segment_start_us)
                                  : 0;
        uint32_t flags = extra_flags;
        if (segment_pkt_index == 0) flags |= AURIS_PKT_FLAG_KEYFRAME;
        slot->flags         = flags;
        slot->size          = static_cast<uint32_t>(n);

        const bool overran = e->packets.commit(slot);
        e->packets_emitted.fetch_add(1, std::memory_order_relaxed);
        ++segment_pkt_index;
        if (overran) {
            e->packets_dropped.fetch_add(1, std::memory_order_relaxed);
            auris_event_t evt{};
            evt.type         = AURIS_EVT_OVERRUN;
            evt.timestamp_us = now_ts;
            emit_event(e, evt);
        }
    };

    // Pre-roll ring. Captures frames while no segment is open so the leading
    // ~pre_roll_ms of every detected clip are recovered. Disabled (capacity
    // 0) if pre_roll_ms == 0.
    auris::PrerollRing preroll;
    const uint32_t preroll_frames = (e->cfg.pre_roll_ms == 0)
        ? 0u
        : (e->cfg.pre_roll_ms + e->cfg.frame_ms - 1u) / e->cfg.frame_ms;
    preroll.init(preroll_frames, frame_samples);

    auto flush_preroll_into_segment = [&]() {
        if (preroll.empty()) return;
        // Move segment_start_us back to cover the pre-roll period so the
        // PREROLL packets get pts starting at 0 and the first post-trigger
        // packet inherits its natural pts.
        segment_start_us = preroll.oldest_ts_us();
        preroll.for_each_oldest_first([&](const auris::PrerollRing::Frame& f) {
            emit_packet(f.samples.data(), f.capture_ts_us, AURIS_PKT_FLAG_PREROLL);
        });
        preroll.clear();
    };

    while (true) {

        bool did_work = false;

        // Drain as many full frames as the ring holds.
        while (e->pcm_ring.size_relaxed() >= frame_samples) {
            const size_t got = e->pcm_ring.pop(scratch.data(), frame_samples);
            if (got < frame_samples) break;   // race-safety fallback
            did_work = true;

            // Deterministic frame clock (see worker_loop top). A frame in hand
            // implies a prior push set first_capture_ts_us; fall back to 0 only
            // defensively.
            uint64_t base_ts = e->first_capture_ts_us.load(std::memory_order_relaxed);
            if (base_ts == ~uint64_t{0}) base_ts = 0;
            const uint64_t now_ts = base_ts + frame_index * frame_us;
            ++frame_index;

            // Noise suppression first, in place: detectors, classifier, and
            // the encoded packet all consume the cleaned frame.
            if (preproc_ready && preproc) {
                preproc->process(scratch.data(), frame_samples);
            }

            // Adopt any retune the host requested (cheap no-op when unchanged).
            refresh_tunables();

            // Per-frame: run both detectors, OR their triggers. With both
            // disabled the worker behaves as step-1 passthrough. `*_ready`
            // means "constructed + initialised"; `*_enabled` is the live toggle.
            auris::DetectionResult vr;
            auris::DetectionResult ar;
            const bool vad_on     = vad_ready     && vad_enabled     && vad;
            const bool anomaly_on = anomaly_ready && anomaly_enabled && anomaly;
            if (vad_on)     vr = vad->process(scratch.data(), frame_samples);
            if (anomaly_on) ar = anomaly->process(scratch.data(), frame_samples);

            // Sound classifier: buffer audio, run inference periodically,
            // and emit AURIS_EVT_SOUND_CLASS when the top-1 probability
            // crosses threshold. Independent of the segment lifecycle; the
            // engine surfaces what was heard, the host decides what to do.
            if (classifier_ready && sound_class_enabled && classifier) {
                const auto cr = classifier->feed(scratch.data(), frame_samples);
                if (cr.ready && cr.confidence >= classifier_threshold &&
                    cr.class_id >= 0) {
                    auris_event_t evt{};
                    evt.type         = AURIS_EVT_SOUND_CLASS;
                    evt.segment_id   = open_segment_id;
                    evt.timestamp_us = now_ts;
                    evt.reason_mask  = static_cast<uint32_t>(cr.class_id);
                    evt.confidence   = cr.confidence;
                    emit_event(e, evt);
                }
            }

            const bool detection_running = (vad_on || anomaly_on);
            const bool is_active         = vr.active || ar.active || !detection_running;
            const bool vad_rising        = vr.active && !vad_was_active;
            const bool ano_rising        = ar.active && !ano_was_active;

            if (is_active) {
                silence_frames = 0;
                if (!segment_open) {
                    segment_open       = true;
                    segment_had_speech = false;
                    // Open the segment, then replay the pre-roll ring so the
                    // leading frames of the clip ride along with the
                    // PREROLL flag. flush_preroll_into_segment moves
                    // segment_start_us backwards so PREROLL packets get
                    // pts = 0..preroll_total_us.
                    open_segment(now_ts, std::max(vr.confidence, ar.confidence));
                    flush_preroll_into_segment();
                }
                if (vad_rising && vad_on) {
                    segment_had_speech = true;
                    auris_event_t evt{};
                    evt.type         = AURIS_EVT_SPEECH_BEGIN;
                    evt.segment_id   = open_segment_id;
                    evt.timestamp_us = now_ts;
                    evt.confidence   = vr.confidence;
                    emit_event(e, evt);
                }
                if (ano_rising && anomaly_on) {
                    auris_event_t evt{};
                    evt.type         = AURIS_EVT_ANOMALY;
                    evt.segment_id   = open_segment_id;
                    evt.timestamp_us = now_ts;
                    evt.reason_mask  = ar.reason_mask;
                    evt.confidence   = ar.confidence;
                    emit_event(e, evt);
                }
                emit_packet(scratch.data(), now_ts, 0u);
            } else if (segment_open) {
                // Trailing silence inside a segment: keep emitting as
                // POSTROLL bytes and count down the hangover. Once exhausted,
                // close.
                emit_packet(scratch.data(), now_ts, AURIS_PKT_FLAG_POSTROLL);
                ++silence_frames;
                if (silence_frames >= hangover_frames) {
                    if (segment_had_speech) {
                        auris_event_t evt{};
                        evt.type         = AURIS_EVT_SPEECH_END;
                        evt.segment_id   = open_segment_id;
                        evt.timestamp_us = now_ts;
                        evt.confidence   = 1.0f;
                        emit_event(e, evt);
                    }
                    close_segment(now_ts);
                    segment_open       = false;
                    segment_had_speech = false;
                    silence_frames     = 0;
                }
            } else {
                // Pre-trigger silence with detection running. Keep the
                // frame in the pre-roll ring so the next segment can recover
                // its onset.
                preroll.push(scratch.data(), now_ts);
            }

            vad_was_active = vr.active;
            ano_was_active = ar.active;
        }

        if (did_work) {
            last_seen_wake = e->wake_count.load(std::memory_order_acquire);
            continue;
        }

        // Drain is complete. Honor stop AFTER draining so any frame the host
        // pushed before calling auris_stop is processed before we exit.
        if (e->stop_requested.load(std::memory_order_acquire)) break;

        // Park on the wake counter until producer signals or stop is requested.
        const uint64_t cur = e->wake_count.load(std::memory_order_acquire);
        if (cur == last_seen_wake) {
            e->wake_count.wait(cur, std::memory_order_acquire);
        }
        last_seen_wake = e->wake_count.load(std::memory_order_acquire);
    }

    // Clean shutdown: balance the state machine. If we left mid-segment
    // with speech still tagged, emit SPEECH_END before SEGMENT_END.
    {
        // Deterministic clock: the timestamp just past the last processed frame.
        uint64_t base_ts = e->first_capture_ts_us.load(std::memory_order_relaxed);
        if (base_ts == ~uint64_t{0}) base_ts = 0;
        const uint64_t now_ts = base_ts + frame_index * frame_us;
        if (segment_open && segment_had_speech) {
            auris_event_t evt{};
            evt.type         = AURIS_EVT_SPEECH_END;
            evt.segment_id   = open_segment_id;
            evt.timestamp_us = now_ts;
            emit_event(e, evt);
        }
        close_segment(now_ts);
    }
}

// Thread entry. The ABI promises no C++ exception ever escapes the library, but
// worker_run allocates (scratch/pre-roll buffers, event deque, detector/encoder
// init) on a library-spawned thread — an escaping exception here would call
// std::terminate and kill the HOST process. Catch everything, flag the engine
// fatal, and surface it as a backend error the host can react to.
void worker_loop(auris_engine* e) noexcept {
    try {
        worker_run(e);
    } catch (...) {
        e->worker_fatal.store(true, std::memory_order_release);
        try {
            auris_event_t evt{};
            evt.type         = AURIS_EVT_BACKEND_ERROR;
            evt.reason_mask  = static_cast<uint32_t>(AURIS_ERR_INTERNAL);
            evt.timestamp_us = e->last_capture_ts_us.load(std::memory_order_relaxed);
            emit_event(e, evt);
        } catch (...) {
            // The error reporter itself may throw (deque alloc); never let that
            // escape the catch handler.
        }
    }
}

} // namespace

// ============================================================================
// C ABI
// ============================================================================

extern "C" {

AURIS_API auris_status_t auris_config_defaults(auris_config_t* out_cfg) {
    if (out_cfg == nullptr) return AURIS_ERR_INVALID_ARG;
    *out_cfg = auris_config_t{};
    out_cfg->backend                = AURIS_BACKEND_AUTO;
    out_cfg->sample_rate_hz         = 16000;
    out_cfg->channels               = kChannelsV1;
    out_cfg->sample_format          = AURIS_SAMPLE_S16;
    out_cfg->frame_ms               = 20;
    out_cfg->enable_rnnoise         = 0;
    out_cfg->enable_vad             = 1;
    out_cfg->vad_tier               = AURIS_VAD_FAST;
    out_cfg->vad_aggressiveness     = 2;
    out_cfg->vad_neural_threshold   = 0.5f;
    out_cfg->vad_hangover_ms        = 300;
    out_cfg->enable_anomaly         = 1;
    out_cfg->anomaly_rms_z          = 3.0f;
    out_cfg->anomaly_flux_z         = 3.0f;
    out_cfg->anomaly_background_ms  = 2000;
    out_cfg->pre_roll_ms            = 300;
    out_cfg->post_roll_ms           = 300;
    out_cfg->segment_max_ms         = 60000;
    // PCM passthrough is the default until libopus is wired in (back half
    // of §8 step 4). At that point this flips back to AURIS_CODEC_OPUS.
    out_cfg->codec                  = AURIS_CODEC_PCM_S16LE;
    out_cfg->opus_bitrate_bps       = 32000;
    out_cfg->opus_vbr               = 1;
    out_cfg->opus_complexity        = 5;
    out_cfg->pcm_ring_frames        = 1u << 14;   // 16384 samples (~1 s @ 16 kHz)
    out_cfg->packet_queue_capacity  = 256;
    out_cfg->enable_wait_handle     = 0;
    out_cfg->neural_vad_model_path  = nullptr;
    out_cfg->enable_sound_class      = 0;
    out_cfg->sound_class_model_path  = nullptr;
    out_cfg->sound_class_threshold   = 0.4f;
    out_cfg->sound_class_interval_ms = 500;
    return AURIS_OK;
}

AURIS_API auris_status_t auris_create(const auris_config_t* cfg,
                                      auris_engine_t**      out_engine) {
    // Always nullify the out pointer before any validation so the caller
    // does not have to special-case error returns.
    if (out_engine != nullptr) *out_engine = nullptr;
    if (cfg == nullptr || out_engine == nullptr) return AURIS_ERR_INVALID_ARG;

    const auris_status_t v = validate_config(*cfg);
    if (v != AURIS_OK) {
        // The config failed validation, so its enum fields may hold
        // out-of-range values — read them as raw ints (loading them as enums
        // would itself be UB).
        AURIS_LOG("validate_config failed: %d (backend=%d codec=%d "
                  "vad_tier=%d sr=%u frame_ms=%u channels=%u)",
                  static_cast<int>(v),
                  enum_as_int(cfg->backend),
                  enum_as_int(cfg->codec),
                  enum_as_int(cfg->vad_tier),
                  cfg->sample_rate_hz, cfg->frame_ms, cfg->channels);
        return v;
    }

    try {
        auto encoder = auris::make_encoder(cfg->codec);
        if (encoder == nullptr) {
            AURIS_LOG("make_encoder(%d) returned null",
                      static_cast<int>(cfg->codec));
            return AURIS_ERR_UNSUPPORTED;
        }
        const auris_status_t enc_init = encoder->init(*cfg);
        if (enc_init != AURIS_OK) {
            AURIS_LOG("encoder->init failed: %d", static_cast<int>(enc_init));
            return enc_init;
        }
        const size_t max_payload = encoder->max_packet_bytes();

        auto e = std::make_unique<auris_engine>(
            *cfg,
            /* ring_frames        */ cfg->pcm_ring_frames,
            /* pool_capacity      */ cfg->packet_queue_capacity,
            /* max_payload_bytes  */ max_payload);

        e->encoder = std::move(encoder);

        // // CONTRACT  Deep-copy the borrowed model-path strings into
        // engine-owned storage and repoint e->cfg at them. The caller's
        // const char* buffers are only valid for the duration of this call;
        // the worker thread reads these paths later, so without owned copies
        // it would dereference freed memory (the bug behind garbage paths
        // like '0\xef\xbf\xbdt' and silent fallback to "silero_vad.onnx").
        if (cfg->neural_vad_model_path != nullptr &&
            cfg->neural_vad_model_path[0] != '\0') {
            e->neural_vad_model_path_storage = cfg->neural_vad_model_path;
            e->cfg.neural_vad_model_path = e->neural_vad_model_path_storage.c_str();
        } else {
            e->cfg.neural_vad_model_path = nullptr;
        }
        if (cfg->sound_class_model_path != nullptr &&
            cfg->sound_class_model_path[0] != '\0') {
            e->sound_class_model_path_storage = cfg->sound_class_model_path;
            e->cfg.sound_class_model_path = e->sound_class_model_path_storage.c_str();
        } else {
            e->cfg.sound_class_model_path = nullptr;
        }

        if (cfg->backend != AURIS_BACKEND_HOST_PCM) {
            e->capture_backend = auris::make_backend(cfg->backend);
            if (e->capture_backend == nullptr) {
                AURIS_LOG("make_backend(%d) returned null",
                          static_cast<int>(cfg->backend));
                return AURIS_ERR_UNSUPPORTED;
            }
        }

        *out_engine = e.release();
        return AURIS_OK;
    } catch (const std::bad_alloc&) {
        return AURIS_ERR_NO_MEMORY;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API void auris_destroy(auris_engine_t* engine) {
    if (engine == nullptr) return;
    try {
        std::lock_guard lk(engine->lifecycle_mu);
        if (engine->running.load(std::memory_order_acquire)) {
            if (engine->capture_backend) {
                engine->capture_backend->stop();
            }
            engine->stop_requested.store(true, std::memory_order_release);
            engine->wake_count.fetch_add(1, std::memory_order_release);
            engine->wake_count.notify_one();
            if (engine->worker_thread.joinable()) {
                engine->worker_thread.join();
            }
            engine->running.store(false, std::memory_order_release);
        }
    } catch (...) {
        // best effort; we must still delete.
    }
    delete engine;
}

AURIS_API auris_status_t auris_start(auris_engine_t* e) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    std::lock_guard lk(e->lifecycle_mu);
    if (e->running.load(std::memory_order_acquire)) return AURIS_ERR_ALREADY_RUNNING;
    try {
        e->stop_requested.store(false, std::memory_order_release);
        e->last_capture_ts_us.store(monotonic_now_us(), std::memory_order_release);
        // Fresh deterministic frame clock for this run; the first push sets the base.
        e->first_capture_ts_us.store(~uint64_t{0}, std::memory_order_release);

        // 1. Prepare the capture backend so we fail fast on init errors
        //    (permission denied, no device) before we spawn the worker.
        if (e->capture_backend) {
            const auris_status_t r = e->capture_backend->init(
                e->cfg, backend_pcm_callback, e);
            if (r != AURIS_OK) return r;
        }

        // 2. Spawn the worker; it idles on wake_count until PCM arrives.
        e->worker_thread = std::thread(worker_loop, e);

        // 3. Start capture last so frames only flow once the consumer is ready.
        if (e->capture_backend) {
            const auris_status_t r = e->capture_backend->start();
            if (r != AURIS_OK) {
                // Tear down the worker we just spawned.
                e->stop_requested.store(true, std::memory_order_release);
                e->wake_count.fetch_add(1, std::memory_order_release);
                e->wake_count.notify_one();
                if (e->worker_thread.joinable()) {
                    e->worker_thread.join();
                }
                return r;
            }
        }

        e->running.store(true, std::memory_order_release);
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_stop(auris_engine_t* e) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    std::lock_guard lk(e->lifecycle_mu);
    if (!e->running.load(std::memory_order_acquire)) return AURIS_ERR_NOT_RUNNING;
    try {
        // 1. Stop capture first. backend->stop() blocks until the data
        //    callback has finished, so no further pushes hit the ring after
        //    this returns.
        if (e->capture_backend) {
            e->capture_backend->stop();
        }

        // 2. Now stop the worker. It will drain whatever remains in the ring
        //    on its next loop iteration before noticing stop_requested.
        e->stop_requested.store(true, std::memory_order_release);
        e->wake_count.fetch_add(1, std::memory_order_release);
        e->wake_count.notify_one();
        if (e->worker_thread.joinable()) {
            e->worker_thread.join();
        }
        e->running.store(false, std::memory_order_release);
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_set_event_callback(auris_engine_t* e,
                                                  auris_event_cb  cb,
                                                  void*           user) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->cb_mu);
        e->event_cb   = cb;
        e->event_user = user;
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_poll_event(auris_engine_t* e, auris_event_t* out) {
    if (e == nullptr || out == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        std::lock_guard lk(e->evt_mu);
        if (e->evt_queue.empty()) return AURIS_ERR_QUEUE_EMPTY;
        *out = e->evt_queue.front();
        e->evt_queue.pop_front();
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_drain(auris_engine_t* e, auris_packet_t* out) {
    if (e == nullptr || out == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        return e->packets.drain(out) ? AURIS_OK : AURIS_ERR_QUEUE_EMPTY;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_release(auris_engine_t* e, const auris_packet_t* pkt) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        return e->packets.release(pkt);
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_update_config(auris_engine_t*       e,
                                             const auris_config_t* cfg) {
    if (e == nullptr || cfg == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        // Full validation first: a retune must never install a config the
        // engine would have rejected at create time (NaN thresholds, etc.).
        const auris_status_t v = validate_config(*cfg);
        if (v != AURIS_OK) return v;

        // Structural fields are fixed for the engine's lifetime — they size
        // buffers, pick backends, and load models. Reject rather than silently
        // ignore, so a caller never believes a change took effect.
        const auris_config_t& c = e->cfg;
        if (enum_as_int(cfg->backend)       != enum_as_int(c.backend)       ||
            enum_as_int(cfg->sample_format) != enum_as_int(c.sample_format) ||
            enum_as_int(cfg->vad_tier)      != enum_as_int(c.vad_tier)      ||
            enum_as_int(cfg->codec)         != enum_as_int(c.codec)         ||
            cfg->sample_rate_hz         != c.sample_rate_hz         ||
            cfg->channels               != c.channels               ||
            cfg->frame_ms               != c.frame_ms               ||
            cfg->enable_rnnoise         != c.enable_rnnoise         ||
            cfg->pre_roll_ms            != c.pre_roll_ms            ||
            cfg->post_roll_ms           != c.post_roll_ms           ||
            cfg->segment_max_ms         != c.segment_max_ms         ||
            cfg->anomaly_background_ms  != c.anomaly_background_ms  ||
            cfg->sound_class_interval_ms != c.sound_class_interval_ms ||
            cfg->pcm_ring_frames        != c.pcm_ring_frames        ||
            cfg->packet_queue_capacity  != c.packet_queue_capacity  ||
            cfg->enable_wait_handle     != c.enable_wait_handle) {
            return AURIS_ERR_INVALID_ARG;
        }
        // A detector that was off at create time was never constructed, so it
        // cannot be switched on later. Turning one off and back on is fine.
        if ((cfg->enable_vad         && !c.enable_vad)         ||
            (cfg->enable_anomaly     && !c.enable_anomaly)     ||
            (cfg->enable_sound_class && !c.enable_sound_class)) {
            return AURIS_ERR_INVALID_ARG;
        }

        {
            std::lock_guard lk(e->tune_mu);
            // Copy ONLY the tunable subset; model-path pointers in live_cfg
            // keep pointing at the engine's own storage.
            e->live_cfg.enable_vad            = cfg->enable_vad;
            e->live_cfg.enable_anomaly        = cfg->enable_anomaly;
            e->live_cfg.enable_sound_class    = cfg->enable_sound_class;
            e->live_cfg.vad_aggressiveness    = cfg->vad_aggressiveness;
            e->live_cfg.vad_neural_threshold  = cfg->vad_neural_threshold;
            e->live_cfg.vad_hangover_ms       = cfg->vad_hangover_ms;
            e->live_cfg.anomaly_rms_z         = cfg->anomaly_rms_z;
            e->live_cfg.anomaly_flux_z        = cfg->anomaly_flux_z;
            e->live_cfg.sound_class_threshold = cfg->sound_class_threshold;
        }
        // Publish after the write: the worker's acquire-load of the generation
        // pairs with this release so it always sees the completed snapshot.
        e->tune_generation.fetch_add(1, std::memory_order_release);
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_get_stats(auris_engine_t* e, auris_stats_t* out) {
    if (e == nullptr || out == nullptr) return AURIS_ERR_INVALID_ARG;
    try {
        out->frames_captured        = e->frames_captured.load(std::memory_order_relaxed);
        out->frames_dropped_capture = e->frames_dropped_capture.load(std::memory_order_relaxed);
        out->packets_emitted        = e->packets_emitted.load(std::memory_order_relaxed);
        out->packets_dropped        = e->packets_dropped.load(std::memory_order_relaxed);
        out->segments_opened        = e->segments_opened.load(std::memory_order_relaxed);
        out->segments_closed        = e->segments_closed.load(std::memory_order_relaxed);
        out->pcm_ring_fill_frames   = static_cast<uint32_t>(e->pcm_ring.size_relaxed());
        out->packet_queue_fill      = static_cast<uint32_t>(e->packets.ready_size());
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_wait_handle(auris_engine_t* e, intptr_t* out) {
    if (e == nullptr || out == nullptr) return AURIS_ERR_INVALID_ARG;
    if (!e->cfg.enable_wait_handle) return AURIS_ERR_UNSUPPORTED;
    // // VERIFY  Per-platform realization deferred to a later step in §8.
    // Returning UNSUPPORTED until the Apple decision (kqueue vs pipe vs
    // dispatch_semaphore_t) and a Linux/Android eventfd path land.
    (void)out;
    return AURIS_ERR_UNSUPPORTED;
}

AURIS_API auris_status_t auris_push_pcm(auris_engine_t* e,
                                        const int16_t*  samples,
                                        uint32_t        frame_count,
                                        uint64_t        capture_ts_us) {
    if (e == nullptr || samples == nullptr) return AURIS_ERR_INVALID_ARG;
    if (e->cfg.backend != AURIS_BACKEND_HOST_PCM) return AURIS_ERR_INVALID_STATE;
    if (e->worker_fatal.load(std::memory_order_acquire)) return AURIS_ERR_INTERNAL;
    if (!e->running.load(std::memory_order_acquire)) return AURIS_ERR_NOT_RUNNING;
    if (frame_count == 0) return AURIS_OK;
    // Bound the sample count to the ring capacity. The ABI can't see the real
    // length of `samples`, so a caller that lies (frame_count far larger than
    // the buffer) would make the ring memcpy read out of bounds. Rejecting an
    // impossible count turns that into a clean error and also stops a single
    // oversized push from silently dropping most of itself while returning OK.
    if (frame_count > e->pcm_ring.capacity()) return AURIS_ERR_INVALID_ARG;

    engine_ingest_pcm(e, samples, frame_count, capture_ts_us);
    return AURIS_OK;
}

} // extern "C"
