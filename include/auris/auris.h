/*
 * auris.h - canonical C ABI for the Auris audio capture + detection engine.
 *
 * This header is THE contract. Implementation details may change; this surface
 * must remain stable across minor versions. Bindings (Dart/Kotlin/Swift/Go/.NET/
 * Python/Rust/Node/WASM) are generated from this file alone.
 *
 * ============================================================================
 * THREADING MODEL
 * ============================================================================
 *
 *   T1  RT capture thread (library)    alloc-free, never blocks, no host calls
 *           | PCM ring buffer    SPSC, lock-free, fixed size
 *           v
 *   T2  Worker thread (library)        preprocess -> VAD + anomaly ->
 *           |                          maintain pre-roll, encode segment
 *           | Encoded packet queue   bounded, drop-oldest on full
 *           v
 *   T3  Host drain thread (caller)     auris_drain / auris_release at any pace
 *
 * The two queues are the ONLY coupling between real-time work and the host.
 * A slow host drains slower; the packet queue drops oldest and raises
 * AURIS_EVT_OVERRUN. T1 and T2 never feel backpressure.
 *
 * ============================================================================
 * OWNERSHIP / SAFETY CONTRACT  (binding authors MUST honor)
 * ============================================================================
 *
 * 1. // NATIVE-FREE
 *    Every successful auris_create() pairs with exactly one auris_destroy().
 *    auris_destroy() is idempotent on NULL.
 *
 * 2. // ZERO-COPY
 *    auris_packet.data is a BORROWED pointer into an engine-owned pool. It is
 *    valid only until the matching auris_release(). The host MUST copy or
 *    forward the bytes, then release promptly. Holding packets indefinitely
 *    will exhaust the pool and force overruns.
 *
 * 3. // ABI-SAFE
 *    No C++ exception ever crosses this surface. All errors flow through
 *    auris_status_t. No errno reads, no thread-local error state.
 *
 * 4. // CONTRACT  Single-host-thread on the drain side.
 *    auris_drain / auris_release / auris_poll_event are safe to call from ONE
 *    host thread concurrent with T1/T2. They are NOT safe to call from
 *    multiple host threads at once on the same engine. The host is responsible
 *    for serializing these calls if it wishes to fan out.
 *
 *    // CONTRACT  Lifecycle calls must be externally serialized.
 *    auris_create / auris_start / auris_stop / auris_push_pcm / auris_get_stats
 *    and especially auris_destroy must not overlap in time on the same engine.
 *    auris_destroy FREES the engine: calling it while any other call is still in
 *    flight (a push in progress, a drain on another thread) is a use-after-free
 *    the library cannot defend against — the guard protecting it lives inside the
 *    object being freed. start/stop are internally serialized against each other
 *    (no double-join), but the host still owns the destroy ordering.
 *
 * 5. // CONTRACT  Event callback discipline.
 *    The callback registered via auris_set_event_callback fires on T2
 *    (the worker thread). It MUST be non-blocking and MUST NOT re-enter the
 *    engine except for auris_drain / auris_release. Garbage-collected
 *    runtimes must marshal across a thread-safe shim (Dart
 *    NativeCallable.listener, JNI AttachCurrentThread, .NET pinned GCHandle).
 *
 * 6. // CONTRACT  Microphone permission is the host's problem.
 *    auris_start() will return AURIS_ERR_PERMISSION if the OS denies capture,
 *    but the engine does not prompt. Host obtains permission first.
 *
 * 7. // CONTRACT  No raw audio is logged or persisted by the library.
 *    The PCM stream is treated as sensitive (potentially voice / private
 *    interior audio). Encoded packets are emitted only via auris_drain.
 *    The library writes nothing to disk and writes no audio bytes to stderr
 *    or syslog.
 *
 * 8. // CONTRACT  Caller-supplied pointers are validated at the boundary.
 *    auris_create() validates every field of auris_config_t. Lengths and
 *    pointers in auris_push_pcm are bounds-checked. Invalid input returns
 *    AURIS_ERR_INVALID_ARG; it never crashes the engine.
 */

#ifndef AURIS_H
#define AURIS_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(AURIS_BUILD_SHARED)
#  ifdef AURIS_BUILDING
#    define AURIS_API __declspec(dllexport)
#  else
#    define AURIS_API __declspec(dllimport)
#  endif
#else
#  define AURIS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * VERSION
 * ========================================================================== */

#define AURIS_VERSION_MAJOR 0
#define AURIS_VERSION_MINOR 1
#define AURIS_VERSION_PATCH 0

/* ============================================================================
 * STATUS CODES
 *
 * // CONTRACT  Distinct error codes must NOT leak information about why
 * something failed in a way that could form a side channel for the host's
 * upstream data. For audio we are not adversarial against the host, so a
 * granular taxonomy is fine here. (Crypto-style error collapsing does not
 * apply to this domain.)
 * ========================================================================== */

typedef enum auris_status {
    AURIS_OK                    =  0,
    AURIS_ERR_INVALID_ARG       = -1,
    AURIS_ERR_INVALID_STATE     = -2,
    AURIS_ERR_NO_MEMORY         = -3,
    AURIS_ERR_BACKEND           = -4,
    AURIS_ERR_NOT_RUNNING       = -5,
    AURIS_ERR_ALREADY_RUNNING   = -6,
    AURIS_ERR_QUEUE_EMPTY       = -7,   /* drain/poll: nothing available now */
    AURIS_ERR_PERMISSION        = -8,
    AURIS_ERR_UNSUPPORTED       = -9,
    AURIS_ERR_INTERNAL          = -10
} auris_status_t;

/* ============================================================================
 * OPAQUE ENGINE HANDLE
 * ========================================================================== */

typedef struct auris_engine auris_engine_t;

/* ============================================================================
 * ENUMS
 * ========================================================================== */

typedef enum auris_backend {
    AURIS_BACKEND_AUTO       = 0,   /* pick best available for the platform */
    AURIS_BACKEND_MINIAUDIO  = 1,   /* portable default */
    AURIS_BACKEND_OBOE       = 2,   /* Android, low latency */
    AURIS_BACKEND_COREAUDIO  = 3,   /* iOS + macOS, low latency */
    AURIS_BACKEND_HOST_PCM   = 4    /* no internal capture; host pushes PCM */
} auris_backend_t;

typedef enum auris_sample_format {
    AURIS_SAMPLE_S16 = 0            /* v1: signed 16-bit interleaved only */
} auris_sample_format_t;

typedef enum auris_vad_tier {
    AURIS_VAD_FAST   = 0,           /* libfvad, runtime default */
    AURIS_VAD_NEURAL = 1            /* Silero via ONNX Runtime, build-flag gated */
} auris_vad_tier_t;

typedef enum auris_codec {
    AURIS_CODEC_PCM_S16LE = 0,      /* passthrough, host-side encode */
    AURIS_CODEC_OPUS      = 1,      /* live path */
    AURIS_CODEC_FLAC      = 2       /* archival (Sentinel) */
} auris_codec_t;

typedef enum auris_event_type {
    AURIS_EVT_SPEECH_BEGIN    = 1,
    AURIS_EVT_SPEECH_END      = 2,
    AURIS_EVT_ANOMALY         = 3,  /* point event; opens its own segment */
    AURIS_EVT_SEGMENT_BEGIN   = 4,
    AURIS_EVT_SEGMENT_END     = 5,
    AURIS_EVT_OVERRUN         = 6,  /* packet queue dropped oldest */
    AURIS_EVT_BACKEND_ERROR   = 7,
    /* Semantic sound classification from YAMNet. reason_mask carries the
     * AudioSet class index (0..520, see yamnet_class_map.txt);
     * confidence is the model's top-1 probability in [0, 1]. Opens a
     * segment when the class is a known anomaly / animal / alarm. */
    AURIS_EVT_SOUND_CLASS     = 8,
    AURIS_EVT_DEGRADATION     = 9   /* reason_mask = new degradation level (0..3),
                                       confidence  = worker busy fraction 0..1 */
} auris_event_type_t;

/* Packet flags. Bitmask. */
enum {
    AURIS_PKT_FLAG_PREROLL  = 1u << 0,  /* pre-segment frames captured before trigger */
    AURIS_PKT_FLAG_POSTROLL = 1u << 1,  /* trailing frames after detector closes */
    AURIS_PKT_FLAG_KEYFRAME = 1u << 2,  /* first encoded frame of a segment */
    AURIS_PKT_FLAG_LAST     = 1u << 3   /* terminal packet of a segment */
};

/* Anomaly trigger reason. Bitmask in auris_event.reason_mask when
 * type == AURIS_EVT_ANOMALY. */
enum {
    AURIS_ANOMALY_RMS           = 1u << 0,
    AURIS_ANOMALY_SPECTRAL_FLUX = 1u << 1,
    AURIS_ANOMALY_CENTROID      = 1u << 2
};

/* ============================================================================
 * CONFIG
 *
 * // CONTRACT  All fields are validated by auris_create. Out-of-range values
 * return AURIS_ERR_INVALID_ARG without partial construction.
 * ========================================================================== */

typedef struct auris_config {
    /* --- Capture ----------------------------------------------------------- */
    auris_backend_t        backend;
    uint32_t               sample_rate_hz;
    /* // VERIFY: libfvad sample-rate constraints. Documented set is
     * {8000, 16000, 32000, 48000} Hz. Confirm against the vendored
     * libfvad version before locking. */
    uint16_t               channels;          /* v1: 1 (mono). Validated. */
    auris_sample_format_t  sample_format;
    uint32_t               frame_ms;
    /* // VERIFY: libfvad frame-length constraints. Documented set is
     * {10, 20, 30} ms. Confirm against vendored version. */

    /* --- Preprocess -------------------------------------------------------- */
    int  enable_rnnoise;                      /* 0 = off, 1 = on */

    /* --- VAD --------------------------------------------------------------- */
    int               enable_vad;             /* 0 = passthrough (all frames flow);
                                               * 1 = drive segments off VAD. */
    auris_vad_tier_t  vad_tier;
    int               vad_aggressiveness;     /* libfvad: 0..3. Ignored for NEURAL. */
    float             vad_neural_threshold;   /* Silero P(speech) threshold, 0..1.
                                               * Ignored for FAST. */
    uint32_t          vad_hangover_ms;        /* trailing silence to confirm SPEECH_END */

    /* --- Anomaly gate ------------------------------------------------------ */
    int       enable_anomaly;
    float     anomaly_rms_z;                  /* z-score over rolling background */
    float     anomaly_flux_z;
    uint32_t  anomaly_background_ms;          /* rolling window length */

    /* --- Segmentation ------------------------------------------------------ */
    uint32_t  pre_roll_ms;                    /* frames retained ahead of trigger */
    uint32_t  post_roll_ms;                   /* trailing frames after close */
    uint32_t  segment_max_ms;                 /* hard cap; 0 = unbounded */

    /* --- Encoder ----------------------------------------------------------- */
    auris_codec_t  codec;
    uint32_t       opus_bitrate_bps;
    /* // VERIFY: libopus bitrate range. Documented as roughly 6000..510000
     * bps. Confirm exact bounds and any per-mode constraints (NB/WB/SWB/FB)
     * against vendored libopus version. */
    int            opus_vbr;                  /* 0 = CBR, 1 = VBR */
    int            opus_complexity;           /* 0..10 */

    /* --- Queues ------------------------------------------------------------ */
    uint32_t  pcm_ring_frames;                /* SPSC PCM ring capacity, frames */
    uint32_t  packet_queue_capacity;          /* bounded packet queue depth */

    /* --- Wait handle ------------------------------------------------------- */
    int       enable_wait_handle;             /* if set, auris_wait_handle is valid */

    /* --- Detector model assets --------------------------------------------- */

    /* Filesystem path to the Silero VAD ONNX model. Read only when
     * vad_tier == AURIS_VAD_NEURAL AND the build was compiled with
     * AURIS_HAVE_NEURAL_VAD. NULL falls back to:
     *   1. AURIS_NEURAL_VAD_MODEL env var
     *   2. "silero_vad.onnx" in the current working directory
     * The path is borrowed only for the auris_create call; the engine
     * copies any string it needs to retain. */
    const char* neural_vad_model_path;

    /* --- Sound classifier (YAMNet) ----------------------------------------- */

    /* 0 = disabled. When set, the engine buffers 3 s of audio at the
     * configured sample rate, runs YAMNet every sound_class_interval_ms,
     * and emits AURIS_EVT_SOUND_CLASS when the top-1 probability for an
     * "interesting" class (animal / alarm / glass / speech / cry) crosses
     * sound_class_threshold. Compiled only when AURIS_HAVE_SOUND_CLASS
     * is defined (same ONNX Runtime path as Neural VAD). */
    int      enable_sound_class;

    /* Filesystem path to the YAMNet ONNX model. NULL falls back to the
     * AURIS_SOUND_CLASS_MODEL env var, then "yamnet_3s.onnx" in the
     * current working directory. */
    const char* sound_class_model_path;

    /* Top-1 probability threshold for emitting AURIS_EVT_SOUND_CLASS.
     * 0 falls back to a sensible default (0.4). */
    float    sound_class_threshold;

    /* How often the classifier runs, in milliseconds. 0 falls back to 500. */
    uint32_t sound_class_interval_ms;

    /* Continuous PCM tap. Packets are segment-gated (they flow only while a
     * detector holds a segment open), so a host that also wants an UNGATED
     * stream — to draw a waveform, or to archive everything while still
     * receiving detections — enables the tap and drains it with
     * auris_read_pcm.
     *
     * The tap carries the same frames the detectors and encoder consume, i.e.
     * AFTER RNNoise when enable_rnnoise is set. It is a bounded ring: if the
     * host does not drain fast enough the newest samples are dropped and
     * auris_stats.tap_samples_dropped counts them. */
    int      enable_pcm_tap;

    /* Tap ring capacity in SAMPLES. 0 falls back to 1 second at the working
     * rate. Rounded up to a power of two internally. */
    uint32_t pcm_tap_frames;

    /* Silence gate. A frame whose input RMS (measured BEFORE noise suppression,
     * same value as auris_stats_t.input_rms_dbfs) is below this level is
     * "quiet". Quiet frames skip RNNoise (they carry nothing to denoise), and
     * the sound classifier skips an inference whose entire 3 s window was
     * quiet (YAMNet would only ever answer "Silence"). VAD and the anomaly
     * detector are NOT gated: they are cheap and their behaviour must not
     * depend on level. Audio itself is never gated — every frame still flows
     * to the tap and the encoder.
     *   0        => default, -60 dBFS (conservative: digital silence / muted
     *               mic; raise to ~-50 to also skip quiet-room tone)
     *   <= -120  => disabled (nothing is ever quiet)
     * Live-tunable via auris_update_config. */
    float    silence_gate_dbfs;

    /* Adaptive degradation. When the worker cannot keep up — it spends more
     * than degrade_busy_ppm of wall time processing over a ~1 s window, or the
     * capture ring dropped frames — the engine sheds the most expensive
     * optional work in steps, and restores it after ~3 calm windows:
     *   level 1: sound-class inference cadence x2
     *   level 2: sound-class inference cadence x4
     *   level 3: sound-class inference suspended (audio still buffered, so
     *            recovery is immediate)
     * Each change emits AURIS_EVT_DEGRADATION (reason_mask = new level,
     * confidence = busy fraction) and is visible in
     * auris_stats_t.degradation_level. Default OFF: the policy reacts to wall-
     * clock timing, so a host that pushes HOST_PCM faster than real time
     * (offline processing, the conformance harness) would otherwise get
     * timing-dependent output. Turn it on for live capture on constrained
     * devices. Live-tunable. */
    int      enable_adaptive_degradation;
    uint32_t degrade_busy_ppm;                /* 0 => 700000 (70%); max 1000000 */

    /* Reserved for future expansion. MUST be zeroed by auris_config_defaults
     * and treated as zero by the engine. Allows additive growth without ABI
     * break. */
    uint32_t  _reserved[5];
} auris_config_t;

/* ============================================================================
 * PACKET (borrowed view)
 *
 * // ZERO-COPY  data is owned by the engine pool. Valid only until the
 * matching auris_release. After release the bytes may be overwritten.
 * ========================================================================== */

typedef struct auris_packet {
    uint64_t       segment_id;                /* monotonic per engine */
    uint64_t       pts_us;                    /* offset from segment begin */
    uint64_t       capture_ts_us;             /* monotonic capture clock */
    uint32_t       flags;                     /* AURIS_PKT_FLAG_* bitmask */
    auris_codec_t  codec;
    const uint8_t* data;                      /* // ZERO-COPY  borrowed */
    uint32_t       size;
    uint32_t       _reserved;                 /* engine-internal cookie.
                                               * Bindings: do not read or modify.
                                               * Pass the packet struct back to
                                               * auris_release unchanged. */
} auris_packet_t;

/* ============================================================================
 * EVENT
 * ========================================================================== */

typedef struct auris_event {
    auris_event_type_t  type;
    uint64_t            segment_id;           /* 0 if not segment-bound */
    uint64_t            timestamp_us;         /* capture clock */
    uint32_t            reason_mask;          /* ANOMALY: AURIS_ANOMALY_* bits.
                                               * SOUND_CLASS: YAMNet class index.
                                               * BACKEND_ERROR: an auris_status_t
                                               *   stored as-is; REINTERPRET the
                                               *   bits as int32_t to recover the
                                               *   negative code (e.g. 0xFFFFFFFC
                                               *   == AURIS_ERR_BACKEND -4). */
    float               confidence;           /* 0..1 where meaningful */
    uint32_t            _reserved;
} auris_event_t;

/* ============================================================================
 * STATS  (observability; never carries audio bytes)
 * ========================================================================== */

typedef struct auris_stats {
    uint64_t  frames_captured;
    uint64_t  frames_dropped_capture;         /* SPSC ring overflow (should be 0) */
    uint64_t  packets_emitted;
    uint64_t  packets_dropped;                /* bounded queue drop-oldest */
    uint64_t  segments_opened;
    uint64_t  segments_closed;
    uint32_t  pcm_ring_fill_frames;
    uint32_t  packet_queue_fill;

    /* Input signal level of the most recently processed frame, in dBFS
     * (0.0 == full scale, negative below; floored at -120.0 for digital
     * silence). Measured on the captured frame BEFORE noise suppression, so it
     * reflects what the microphone actually received — the right value to drive
     * a level meter. Both are 0.0 until the first frame is processed. */
    float     input_rms_dbfs;
    float     input_peak_dbfs;

    /* Samples the continuous PCM tap had to drop because the host did not drain
     * it fast enough. Always 0 when enable_pcm_tap is 0. */
    uint64_t  tap_samples_dropped;

    /* Worker health. busy_ppm is the share of wall time the worker spent
     * processing frames over the most recent ~1 s window (parts per million;
     * 1000000 == saturated). max_frame_us is the longest single frame since
     * start. wakeups counts worker thread parks/unparks — on mobile, wakeup
     * rate is a better battery proxy than CPU share. */
    uint32_t  worker_busy_ppm;
    uint32_t  worker_max_frame_us;
    uint64_t  worker_wakeups;

    /* Work-shedding counters. `skipped` counts inferences that were due but
     * gated (silence gate or degradation); preproc_frames_skipped counts
     * frames RNNoise did not run on because they were quiet. */
    uint64_t  sound_class_inferences;
    uint64_t  sound_class_inferences_skipped;
    uint64_t  preproc_frames_skipped;

    /* Current adaptive-degradation level (0 = nominal .. 3 = classifier
     * suspended). Always 0 unless enable_adaptive_degradation is set. */
    uint32_t  degradation_level;

    uint32_t  _reserved[7];                   /* additive growth; always 0 */
} auris_stats_t;

/* ============================================================================
 * EVENT CALLBACK TYPE
 *
 * // CONTRACT  Fires on T2 (worker thread). Non-blocking. Must not re-enter
 * the engine except auris_drain / auris_release. Pointer to evt is borrowed
 * for the duration of the call.
 * ========================================================================== */

typedef void (*auris_event_cb)(const auris_event_t* evt, void* user);

/* ============================================================================
 * LIFECYCLE
 * ========================================================================== */

/* Populate cfg with safe, documented defaults. Zeroes reserved fields.
 * Returns AURIS_ERR_INVALID_ARG if out_cfg is NULL. */
AURIS_API auris_status_t auris_config_defaults(auris_config_t* out_cfg);

/* Construct an engine. Validates every field of cfg.
 * On success *out_engine is non-NULL and ownership transfers to caller.
 * On failure *out_engine is set to NULL and nothing is allocated.
 *
 * // NATIVE-FREE  pair with auris_destroy.
 * // ABI-SAFE    no exception escapes. */
AURIS_API auris_status_t auris_create(const auris_config_t* cfg,
                                      auris_engine_t**      out_engine);

/* Tear down. Stops capture if running, flushes nothing, frees pools.
 * Safe on NULL. After this call the handle is invalid. */
AURIS_API void auris_destroy(auris_engine_t* engine);

/* ============================================================================
 * CONTROL
 * ========================================================================== */

/* Begin capture + detection. Idempotent: returns AURIS_ERR_ALREADY_RUNNING if
 * already started. Returns AURIS_ERR_PERMISSION if the OS denies microphone
 * access (the library does not prompt). */
AURIS_API auris_status_t auris_start(auris_engine_t* engine);

/* Stop capture. Drains in-flight encoded segments before returning. After
 * stop, queued packets remain available via auris_drain until released. */
AURIS_API auris_status_t auris_stop(auris_engine_t* engine);

/* ============================================================================
 * EVENTS  (pick push OR pull; both are wired)
 * ========================================================================== */

/* Push model: register a callback. Pass cb == NULL to unregister.
 * // CONTRACT  Callback fires on T2; see callback discipline above. */
AURIS_API auris_status_t auris_set_event_callback(auris_engine_t* engine,
                                                  auris_event_cb  cb,
                                                  void*           user);

/* Pull model: copy the next event into *out_evt.
 * Returns AURIS_ERR_QUEUE_EMPTY when nothing is pending (non-blocking). */
AURIS_API auris_status_t auris_poll_event(auris_engine_t*  engine,
                                          auris_event_t*   out_evt);

/* ============================================================================
 * SEPARATED SEND PATH
 *
 * // ZERO-COPY  data is borrowed. The host must copy/forward then call
 * auris_release with the same packet struct. Holding more than
 * packet_queue_capacity packets at once will exhaust the pool and force
 * overruns on the producing side.
 * ========================================================================== */

/* Pop the next encoded packet. Returns AURIS_ERR_QUEUE_EMPTY if none.
 * out_pkt.data points into the engine pool. Treat as immutable. */
AURIS_API auris_status_t auris_drain(auris_engine_t*  engine,
                                     auris_packet_t*  out_pkt);

/* Return a previously drained packet to the pool. The same struct (with the
 * same .data pointer) must be passed back. Passing a foreign pointer is
 * undefined: returns AURIS_ERR_INVALID_ARG when detectable. */
AURIS_API auris_status_t auris_release(auris_engine_t*        engine,
                                       const auris_packet_t*  pkt);

/* Drain the continuous PCM tap (requires config.enable_pcm_tap).
 *
 * Copies up to max_samples int16 mono samples into out and writes the count to
 * out_written. Unlike auris_drain, this is NOT gated by segments: it delivers
 * every frame the worker processed, so a host can render a waveform or archive
 * the full stream while still receiving detections and encoded packets.
 *
 * Returns AURIS_OK (out_written may be 0 when nothing is buffered),
 * AURIS_ERR_UNSUPPORTED if the tap is disabled, or AURIS_ERR_INVALID_ARG on a
 * null argument. The host owns `out`; nothing is borrowed, so there is no
 * release call. Drain from ONE thread (same single-consumer contract as
 * auris_drain). */
AURIS_API auris_status_t auris_read_pcm(auris_engine_t* engine,
                                        int16_t*        out,
                                        uint32_t        max_samples,
                                        uint32_t*       out_written);

/* ============================================================================
 * RUNTIME RECONFIGURATION
 * ========================================================================== */

/* Retune a RUNNING (or stopped) engine in place, without destroying it.
 *
 * Pass a full auris_config_t: the STRUCTURAL fields must match the ones the
 * engine was created with, and only the TUNABLE subset is applied:
 *
 *   tunable    enable_vad, enable_anomaly, enable_sound_class,
 *              vad_aggressiveness, vad_neural_threshold, vad_hangover_ms,
 *              anomaly_rms_z, anomaly_flux_z, sound_class_threshold
 *
 *   structural everything else (backend, rate, channels, frame_ms, codec,
 *              vad_tier, model paths, rnnoise, roll/segment/background/interval
 *              durations, queue sizes, wait handle) — these size buffers, pick
 *              backends, and load models, so they are fixed for the engine's
 *              lifetime.
 *
 * The easiest correct call is: take the config you passed to auris_create,
 * change a tunable field, and pass it back.
 *
 * Returns AURIS_ERR_INVALID_ARG if the config fails normal validation, if any
 * structural field differs, or if it tries to switch ON a detector that was OFF
 * at create time (that detector was never constructed, so it cannot start).
 * Turning a detector off and back on again is supported.
 *
 * The change is applied by the worker between frames: detector state (VAD
 * hysteresis, the anomaly rolling background, the classifier's EMA) is
 * PRESERVED, so retuning mid-utterance does not restart detection or reload
 * models. Safe to call from any thread; it does not block the worker. */
AURIS_API auris_status_t auris_update_config(auris_engine_t*       engine,
                                             const auris_config_t* cfg);

/* ============================================================================
 * OBSERVABILITY
 * ========================================================================== */

AURIS_API auris_status_t auris_get_stats(auris_engine_t*  engine,
                                         auris_stats_t*   out_stats);

/* ============================================================================
 * OPTIONAL WAIT HANDLE
 *
 * Lets the host block instead of spin-polling auris_drain.
 *
 * // VERIFY  Per-platform realization of intptr_t:
 *   Linux:   int  (eventfd)
 *   Android: int  (eventfd)
 *   Apple (iOS + macOS): DECISION PENDING. Candidates:
 *     - int read end of a pipe (portable, simple)
 *     - kqueue ident
 *     - dispatch_semaphore_t as void*
 *   Windows: HANDLE  (not in v1 scope; macros guarded for later)
 *
 * If cfg.enable_wait_handle was 0, returns AURIS_ERR_UNSUPPORTED. */
AURIS_API auris_status_t auris_wait_handle(auris_engine_t*  engine,
                                           intptr_t*        out_handle);

/* ============================================================================
 * HOST PCM PUSH  (only when backend == AURIS_BACKEND_HOST_PCM)
 *
 * Lets a host with its own capture path feed PCM directly. Returns
 * AURIS_ERR_INVALID_STATE if the engine was created with a different backend.
 *
 * // CONTRACT  samples is borrowed for the duration of the call. The engine
 * copies into its ring before returning. capture_ts_us is the monotonic
 * timestamp of the FIRST sample in the buffer.
 *
 * Returns AURIS_ERR_NO_MEMORY equivalent (logged as ring overflow in stats)
 * if the SPSC ring is full; the engine drops the oldest frames silently
 * to keep capture lock-free. */
AURIS_API auris_status_t auris_push_pcm(auris_engine_t*  engine,
                                        const int16_t*   samples,
                                        uint32_t         frame_count,
                                        uint64_t         capture_ts_us);

/* ============================================================================
 * UTILITIES  (pure, thread-safe, no allocation)
 * ========================================================================== */

/* Human-readable status string. Owned by the library; do not free.
 * Accepts any int so hostile callers can pass out-of-range values without
 * triggering enum-domain UB; returns "AURIS_UNKNOWN" for unrecognised codes. */
AURIS_API const char* auris_status_str(int s);

/* "X.Y.Z" of the linked library at runtime. Owned by the library. */
AURIS_API const char* auris_version_string(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AURIS_H */
