/*
 * mockingbird.h - canonical C ABI for the Mockingbird audio capture + detection engine.
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
 *   T3  Host drain thread (caller)     mockingbird_drain / mockingbird_release at any pace
 *
 * The two queues are the ONLY coupling between real-time work and the host.
 * A slow host drains slower; the packet queue drops oldest and raises
 * MOCKINGBIRD_EVT_OVERRUN. T1 and T2 never feel backpressure.
 *
 * ============================================================================
 * OWNERSHIP / SAFETY CONTRACT  (binding authors MUST honor)
 * ============================================================================
 *
 * 1. Create and destroy pair up.
 *    Every successful mockingbird_create() pairs with exactly one mockingbird_destroy().
 *    mockingbird_destroy() is idempotent on NULL.
 *
 * 2. Packets are borrowed.
 *    mockingbird_packet.data is a BORROWED pointer into an engine-owned pool. It is
 *    valid only until the matching mockingbird_release(). The host MUST copy or
 *    forward the bytes, then release promptly. Holding packets indefinitely
 *    will exhaust the pool and force overruns.
 *
 * 3. No exception crosses the ABI.
 *    No C++ exception ever crosses this surface. All errors flow through
 *    mockingbird_status_t. No errno reads, no thread-local error state.
 *
 * 4. One host thread drains.
 *    mockingbird_drain / mockingbird_release / mockingbird_poll_event are safe to call from ONE
 *    host thread concurrent with T1/T2. They are NOT safe to call from
 *    multiple host threads at once on the same engine. The host is responsible
 *    for serializing these calls if it wishes to fan out.
 *
 *    Lifecycle calls are serialized by the host.
 *    mockingbird_create / mockingbird_start / mockingbird_stop / mockingbird_push_pcm / mockingbird_get_stats
 *    and especially mockingbird_destroy must not overlap in time on the same engine.
 *    mockingbird_destroy FREES the engine: calling it while any other call is still in
 *    flight (a push in progress, a drain on another thread) is a use-after-free
 *    the library cannot defend against: the guard protecting it lives inside the
 *    object being freed. start/stop are internally serialized against each other
 *    (no double-join), but the host still owns the destroy ordering.
 *
 * 5. The event callback stays short.
 *    The callback registered via mockingbird_set_event_callback fires on T2
 *    (the worker thread). It MUST be non-blocking and MUST NOT re-enter the
 *    engine except for mockingbird_drain / mockingbird_release. Garbage-collected
 *    runtimes must marshal across a thread-safe shim (Dart
 *    NativeCallable.listener, JNI AttachCurrentThread, .NET pinned GCHandle).
 *
 * 6. Microphone permission belongs to the host.
 *    mockingbird_start() will return MOCKINGBIRD_ERR_PERMISSION if the OS denies capture,
 *    but the engine does not prompt. Host obtains permission first.
 *
 * 7. No raw audio is logged or stored by the library, and nothing is sent
 *    anywhere. Before its first ONNX Runtime environment exists the engine
 *    sets ORT_DISABLE_TELEMETRY=1, unless the host set that variable first,
 *    and it disables ONNX Runtime's telemetry events.
 *    The PCM stream is treated as sensitive (potentially voice / private
 *    interior audio). Encoded packets are emitted only via mockingbird_drain.
 *    The library writes nothing to disk and writes no audio bytes to stderr
 *    or syslog.
 *
 * 8. Caller input is validated at the boundary.
 *    mockingbird_create() validates every field of mockingbird_config_t. Lengths and
 *    pointers in mockingbird_push_pcm are bounds-checked. Invalid input returns
 *    MOCKINGBIRD_ERR_INVALID_ARG; it never crashes the engine.
 */

#ifndef MOCKINGBIRD_H
#define MOCKINGBIRD_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(MOCKINGBIRD_BUILDING) && defined(MOCKINGBIRD_BUILD_SHARED)
#    define MOCKINGBIRD_API __declspec(dllexport)
#  else
#    define MOCKINGBIRD_API
#  endif
#else
#  define MOCKINGBIRD_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * VERSION
 * ========================================================================== */

#define MOCKINGBIRD_VERSION_MAJOR 0
#define MOCKINGBIRD_VERSION_MINOR 1
#define MOCKINGBIRD_VERSION_PATCH 0

/* ============================================================================
 * STATUS CODES
 *
 * Each failure has its own code. The host is not an adversary of the audio
 * it captures, so there is no reason to collapse failures into one code.
 * ========================================================================== */

typedef enum mockingbird_status {
    MOCKINGBIRD_OK                    =  0,
    MOCKINGBIRD_ERR_INVALID_ARG       = -1,
    MOCKINGBIRD_ERR_INVALID_STATE     = -2,
    MOCKINGBIRD_ERR_NO_MEMORY         = -3,
    MOCKINGBIRD_ERR_BACKEND           = -4,
    MOCKINGBIRD_ERR_NOT_RUNNING       = -5,
    MOCKINGBIRD_ERR_ALREADY_RUNNING   = -6,
    MOCKINGBIRD_ERR_QUEUE_EMPTY       = -7,   /* drain/poll: nothing available now */
    MOCKINGBIRD_ERR_PERMISSION        = -8,
    MOCKINGBIRD_ERR_UNSUPPORTED       = -9,
    MOCKINGBIRD_ERR_INTERNAL          = -10
} mockingbird_status_t;

/* ============================================================================
 * OPAQUE ENGINE HANDLE
 * ========================================================================== */

typedef struct mockingbird_engine mockingbird_engine_t;

/* ============================================================================
 * ENUMS
 * ========================================================================== */

typedef enum mockingbird_backend {
    MOCKINGBIRD_BACKEND_AUTO       = 0,   /* pick best available for the platform */
    MOCKINGBIRD_BACKEND_MINIAUDIO  = 1,   /* portable default */
    MOCKINGBIRD_BACKEND_OBOE       = 2,   /* Android, low latency */
    MOCKINGBIRD_BACKEND_COREAUDIO  = 3,   /* iOS + macOS, low latency */
    MOCKINGBIRD_BACKEND_HOST_PCM   = 4    /* no internal capture; host pushes PCM */
} mockingbird_backend_t;

typedef enum mockingbird_sample_format {
    MOCKINGBIRD_SAMPLE_S16 = 0            /* v1: signed 16-bit interleaved only */
} mockingbird_sample_format_t;

typedef enum mockingbird_vad_tier {
    MOCKINGBIRD_VAD_FAST   = 0,           /* libfvad, runtime default */
    MOCKINGBIRD_VAD_NEURAL = 1            /* Silero via ONNX Runtime, build-flag gated */
} mockingbird_vad_tier_t;

typedef enum mockingbird_codec {
    MOCKINGBIRD_CODEC_PCM_S16LE = 0,      /* passthrough, host-side encode */
    MOCKINGBIRD_CODEC_OPUS      = 1,      /* raw Opus packets, live path */
    MOCKINGBIRD_CODEC_FLAC      = 2,      /* reserved, not built in */
    /* Opus in Ogg (RFC 7845), one page per packet. A segment's first packet
     * starts with the OpusHead and OpusTags pages and its LAST packet ends
     * the stream, so concatenating one segment's payloads in order gives a
     * playable .opus file. The stream serial is the segment id. */
    MOCKINGBIRD_CODEC_OGG_OPUS  = 3
} mockingbird_codec_t;

typedef enum mockingbird_event_type {
    MOCKINGBIRD_EVT_SPEECH_BEGIN    = 1,
    MOCKINGBIRD_EVT_SPEECH_END      = 2,
    MOCKINGBIRD_EVT_ANOMALY         = 3,  /* point event; opens its own segment */
    MOCKINGBIRD_EVT_SEGMENT_BEGIN   = 4,
    MOCKINGBIRD_EVT_SEGMENT_END     = 5,
    MOCKINGBIRD_EVT_OVERRUN         = 6,  /* packet queue dropped oldest */
    MOCKINGBIRD_EVT_BACKEND_ERROR   = 7,
    /* Semantic sound classification from YAMNet. reason_mask carries the
     * AudioSet class index (0..520, see yamnet_class_map.txt);
     * confidence is the model's top-1 probability in [0, 1]. Opens a
     * segment when the class is a known anomaly / animal / alarm. */
    MOCKINGBIRD_EVT_SOUND_CLASS     = 8,
    MOCKINGBIRD_EVT_DEGRADATION     = 9,  /* reason_mask = new degradation level (0..3),
                                       confidence  = worker busy fraction 0..1 */
    /* The operating system took the microphone away, for a phone call or
     * another app's recording; no frames arrive until CAPTURE_RESUMED. */
    MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED = 10,
    /* Capture is running again after an interruption or a route change. */
    MOCKINGBIRD_EVT_CAPTURE_RESUMED     = 11,
    /* The input device changed (a headset plugged or unplugged, the default
     * device switched); the engine reopened capture on the new route. */
    MOCKINGBIRD_EVT_ROUTE_CHANGED       = 12
} mockingbird_event_type_t;

/* Bits of mockingbird_config_t.voice_processing. */
enum {
    MOCKINGBIRD_VOICE_ECHO_CANCEL    = 1u << 0,  /* remove the device's own playback */
    MOCKINGBIRD_VOICE_GAIN_CONTROL   = 1u << 1,  /* automatic input gain */
    MOCKINGBIRD_VOICE_NOISE_SUPPRESS = 1u << 2   /* the platform's noise suppressor */
};

/* Packet flags. Bitmask. */
enum {
    MOCKINGBIRD_PKT_FLAG_PREROLL  = 1u << 0,  /* pre-segment frames captured before trigger */
    MOCKINGBIRD_PKT_FLAG_POSTROLL = 1u << 1,  /* trailing frames after detector closes */
    MOCKINGBIRD_PKT_FLAG_KEYFRAME = 1u << 2,  /* first encoded frame of a segment */
    MOCKINGBIRD_PKT_FLAG_LAST     = 1u << 3   /* terminal packet of a segment. Set on the
                                               * frame that closes it after trailing
                                               * silence or at segment_max_ms. When stop
                                               * cuts a segment short, only an encoder
                                               * with a container to finish (Ogg Opus)
                                               * emits a LAST packet; SEGMENT_END always
                                               * arrives. */
};

/* Anomaly trigger reason. Bitmask in mockingbird_event.reason_mask when
 * type == MOCKINGBIRD_EVT_ANOMALY. */
enum {
    MOCKINGBIRD_ANOMALY_RMS           = 1u << 0,
    MOCKINGBIRD_ANOMALY_SPECTRAL_FLUX = 1u << 1,
    MOCKINGBIRD_ANOMALY_CENTROID      = 1u << 2
};

/* ============================================================================
 * CONFIG
 *
 * All fields are validated by mockingbird_create. Out-of-range values
 * return MOCKINGBIRD_ERR_INVALID_ARG without partial construction.
 * ========================================================================== */

typedef struct mockingbird_config {
    /* --- Capture ----------------------------------------------------------- */
    mockingbird_backend_t        backend;
    uint32_t               sample_rate_hz;    /* 8000, 16000, 32000 or 48000 */
    uint16_t               channels;          /* v1: 1 (mono). Validated. */
    mockingbird_sample_format_t  sample_format;
    uint32_t               frame_ms;          /* 10, 20 or 30 */

    /* --- Preprocess -------------------------------------------------------- */
    int  enable_rnnoise;                      /* 0 = off, 1 = on */

    /* --- VAD --------------------------------------------------------------- */
    int               enable_vad;             /* 0 = passthrough (all frames flow);
                                               * 1 = drive segments off VAD. */
    mockingbird_vad_tier_t  vad_tier;
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
    uint32_t  post_roll_ms;                   /* trailing audio kept after the detectors
                                               * go quiet; the segment closes after the
                                               * longer of this and vad_hangover_ms */
    uint32_t  segment_max_ms;                 /* hard cap: the frame that reaches it is
                                               * LAST and the next active frame opens a
                                               * new segment; 0 = unbounded */

    /* --- Encoder ----------------------------------------------------------- */
    mockingbird_codec_t  codec;
    uint32_t       opus_bitrate_bps;          /* 6000..510000, for OPUS and OGG_OPUS */
    int            opus_vbr;                  /* 0 = CBR, 1 = VBR */
    int            opus_complexity;           /* 0..10 */

    /* --- Queues ------------------------------------------------------------ */
    uint32_t  pcm_ring_frames;                /* SPSC PCM ring capacity, frames */
    uint32_t  packet_queue_capacity;          /* bounded packet queue depth */

    /* --- Wait handle ------------------------------------------------------- */
    int       enable_wait_handle;             /* if set, mockingbird_wait_handle is valid */

    /* --- Detector model assets --------------------------------------------- */

    /* Filesystem path to the Silero VAD ONNX model. Read only when
     * vad_tier == MOCKINGBIRD_VAD_NEURAL AND the build was compiled with
     * MOCKINGBIRD_HAVE_NEURAL_VAD. NULL falls back to:
     *   1. MOCKINGBIRD_NEURAL_VAD_MODEL env var
     *   2. "silero_vad.onnx" in the current working directory
     * The path is borrowed only for the mockingbird_create call; the engine
     * copies any string it needs to retain. */
    const char* neural_vad_model_path;

    /* --- Sound classifier (YAMNet) ----------------------------------------- */

    /* 0 = disabled. When set, the engine buffers 3 s of audio at the
     * configured sample rate, runs YAMNet every sound_class_interval_ms,
     * and emits MOCKINGBIRD_EVT_SOUND_CLASS when the top-1 probability for an
     * "interesting" class (animal / alarm / glass / speech / cry) crosses
     * sound_class_threshold. Compiled only when MOCKINGBIRD_HAVE_SOUND_CLASS
     * is defined (same ONNX Runtime path as Neural VAD). */
    int      enable_sound_class;

    /* Filesystem path to the YAMNet ONNX model. NULL falls back to the
     * MOCKINGBIRD_SOUND_CLASS_MODEL env var, then "yamnet_3s.onnx" in the
     * current working directory. */
    const char* sound_class_model_path;

    /* Top-1 probability threshold for emitting MOCKINGBIRD_EVT_SOUND_CLASS.
     * 0 falls back to a sensible default (0.4). */
    float    sound_class_threshold;

    /* How often the classifier runs, in milliseconds. 0 falls back to 500. */
    uint32_t sound_class_interval_ms;

    /* Continuous PCM tap. Packets are segment-gated (they flow only while a
     * detector holds a segment open), so a host that also wants an UNGATED
     * stream (to draw a waveform, or to archive everything while still
     * receiving detections) enables the tap and drains it with
     * mockingbird_read_pcm.
     *
     * The tap carries the same frames the detectors and encoder consume, i.e.
     * AFTER RNNoise when enable_rnnoise is set. It is a bounded ring: if the
     * host does not drain fast enough the newest samples are dropped and
     * mockingbird_stats.tap_samples_dropped counts them. */
    int      enable_pcm_tap;

    /* Tap ring capacity in SAMPLES. 0 falls back to 1 second at the working
     * rate. Rounded up to a power of two internally. */
    uint32_t pcm_tap_frames;

    /* Silence gate. A frame whose input RMS (measured BEFORE noise suppression,
     * same value as mockingbird_stats_t.input_rms_dbfs) is below this level is
     * "quiet". Quiet frames skip RNNoise (they carry nothing to denoise), and
     * the sound classifier skips an inference whose entire 3 s window was
     * quiet (YAMNet would only ever answer "Silence"). VAD and the anomaly
     * detector are NOT gated: they are cheap and their behaviour must not
     * depend on level. Audio itself is never gated: every frame still flows
     * to the tap and the encoder.
     *   0        => default, -60 dBFS (conservative: digital silence / muted
     *               mic; raise to ~-50 to also skip quiet-room tone)
     *   <= -120  => disabled (nothing is ever quiet)
     * Live-tunable via mockingbird_update_config. */
    float    silence_gate_dbfs;

    /* Adaptive degradation. When the worker cannot keep up (it spends more
     * than degrade_busy_ppm of wall time processing over a ~1 s window, or the
     * capture ring dropped frames), the engine sheds the most expensive
     * optional work in steps, and restores it after ~3 calm windows:
     *   level 1: sound-class inference cadence x2
     *   level 2: sound-class inference cadence x4
     *   level 3: sound-class inference suspended (audio still buffered, so
     *            recovery is immediate)
     * Each change emits MOCKINGBIRD_EVT_DEGRADATION (reason_mask = new level,
     * confidence = busy fraction) and is visible in
     * mockingbird_stats_t.degradation_level. Default OFF: the policy reacts to wall-
     * clock timing, so a host that pushes HOST_PCM faster than real time
     * (offline processing, the conformance harness) would otherwise get
     * timing-dependent output. Turn it on for live capture on constrained
     * devices. Live-tunable. */
    int      enable_adaptive_degradation;
    uint32_t degrade_busy_ppm;                /* 0 => 700000 (70%); max 1000000 */

    /* Worker wake batching. The capture thread normally wakes the worker on
     * every push (every 10-20 ms frame). With N > 1 it wakes the worker only
     * once at least N frames are queued, so the worker runs N frames per wake
     * and sleeps N times longer between them. CPU work is unchanged; what
     * drops is the wakeup rate, which on phones is what timer coalescing and
     * the scheduler reward with battery. Cost: up to (N-1) frames of added
     * detection latency. 0 or 1 = wake per push. Bounded so the ring can
     * always hold a batch; live-tunable. */
    uint32_t worker_batch_frames;

    /* Platform voice processing for live capture, a MOCKINGBIRD_VOICE_* mask;
     * 0 (the default) captures the raw microphone. iOS and macOS switch to
     * the VoiceProcessingIO unit, Android to the VOICE_COMMUNICATION input
     * preset, and the web passes the bits as getUserMedia constraints. Linux
     * and Windows have no platform processing and ignore it. Each platform
     * applies what it can; mockingbird_stats_t.voice_processing_active says
     * which bits are in effect. Ignored for HOST_PCM. Not live-tunable. */
    uint32_t  voice_processing;

    /* Reserved for future expansion. MUST be zeroed by mockingbird_config_defaults
     * and treated as zero by the engine. Allows additive growth without ABI
     * break. */
    uint32_t  _reserved[3];
} mockingbird_config_t;

/* ============================================================================
 * PACKET (borrowed view)
 *
 * The data pointer is owned by the engine pool. Valid only until the
 * matching mockingbird_release. After release the bytes may be overwritten.
 * ========================================================================== */

typedef struct mockingbird_packet {
    uint64_t       segment_id;                /* monotonic per engine */
    uint64_t       pts_us;                    /* offset from segment begin */
    uint64_t       capture_ts_us;             /* monotonic capture clock */
    uint32_t       flags;                     /* MOCKINGBIRD_PKT_FLAG_* bitmask */
    mockingbird_codec_t  codec;
    const uint8_t* data;                      /* borrowed */
    uint32_t       size;
    uint32_t       _reserved;                 /* engine-internal cookie.
                                               * Bindings: do not read or modify.
                                               * Pass the packet struct back to
                                               * mockingbird_release unchanged. */
} mockingbird_packet_t;

/* ============================================================================
 * EVENT
 * ========================================================================== */

typedef struct mockingbird_event {
    mockingbird_event_type_t  type;
    uint64_t            segment_id;           /* 0 if not segment-bound */
    uint64_t            timestamp_us;         /* capture clock */
    uint32_t            reason_mask;          /* ANOMALY: MOCKINGBIRD_ANOMALY_* bits.
                                               * SOUND_CLASS: YAMNet class index.
                                               * BACKEND_ERROR: a mockingbird_status_t
                                               *   stored as-is; REINTERPRET the
                                               *   bits as int32_t to recover the
                                               *   negative code (e.g. 0xFFFFFFFC
                                               *   == MOCKINGBIRD_ERR_BACKEND -4). */
    float               confidence;           /* 0..1 where meaningful */
    uint32_t            _reserved;
} mockingbird_event_t;

/* ============================================================================
 * STATS  (observability; never carries audio bytes)
 * ========================================================================== */

typedef struct mockingbird_stats {
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
     * reflects what the microphone actually received, which is the right value to drive
     * a level meter. Both are 0.0 until the first frame is processed. */
    float     input_rms_dbfs;
    float     input_peak_dbfs;

    /* Samples the continuous PCM tap had to drop because the host did not drain
     * it fast enough. Always 0 when enable_pcm_tap is 0. */
    uint64_t  tap_samples_dropped;

    /* Worker health. busy_ppm is the share of wall time the worker spent
     * processing frames over the most recent ~1 s window (parts per million;
     * 1000000 == saturated). max_frame_us is the longest single frame since
     * start. wakeups counts worker thread parks/unparks; on mobile, wakeup
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

    /* The MOCKINGBIRD_VOICE_* bits the capture backend applied, which can be
     * fewer than requested. 0 for HOST_PCM and before the first start. */
    uint32_t  voice_processing_active;

    /* Times capture was reopened after an interruption or route change. */
    uint32_t  capture_restarts;

    uint32_t  _reserved[5];                   /* additive growth; always 0 */
} mockingbird_stats_t;

/* ============================================================================
 * EVENT CALLBACK TYPE
 *
 * Fires on T2 (worker thread). Non-blocking. Must not re-enter
 * the engine except mockingbird_drain / mockingbird_release. Pointer to evt is borrowed
 * for the duration of the call.
 * ========================================================================== */

typedef void (*mockingbird_event_cb)(const mockingbird_event_t* evt, void* user);

/* A wake carries no data: it says the event or packet queue gained an entry. */
typedef void (*mockingbird_wake_cb)(void* user);

/* ============================================================================
 * LIFECYCLE
 * ========================================================================== */

/* Populate cfg with safe, documented defaults. Zeroes reserved fields.
 * Returns MOCKINGBIRD_ERR_INVALID_ARG if out_cfg is NULL. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_config_defaults(mockingbird_config_t* out_cfg);

/* Construct an engine. Validates every field of cfg.
 * On success *out_engine is non-NULL and ownership transfers to caller.
 * On failure *out_engine is set to NULL and nothing is allocated.
 *
 * Pair with mockingbird_destroy.
 * No exception escapes. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_create(const mockingbird_config_t* cfg,
                                      mockingbird_engine_t**      out_engine);

/* Tear down. Stops capture if running, flushes nothing, frees pools.
 * Safe on NULL. After this call the handle is invalid. */
MOCKINGBIRD_API void mockingbird_destroy(mockingbird_engine_t* engine);

/* ============================================================================
 * CONTROL
 * ========================================================================== */

/* Begin capture + detection. Idempotent: returns MOCKINGBIRD_ERR_ALREADY_RUNNING if
 * already started. Returns MOCKINGBIRD_ERR_PERMISSION if the OS denies microphone
 * access (the library does not prompt). */
MOCKINGBIRD_API mockingbird_status_t mockingbird_start(mockingbird_engine_t* engine);

/* Stop capture. Drains in-flight encoded segments before returning. After
 * stop, queued packets remain available via mockingbird_drain until released. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_stop(mockingbird_engine_t* engine);

/* ============================================================================
 * EVENTS  (pick push OR pull; both are wired)
 * ========================================================================== */

/* Push model: register a callback. Pass cb == NULL to unregister.
 * Callback fires on T2; see callback discipline above. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_set_event_callback(mockingbird_engine_t* engine,
                                                  mockingbird_event_cb  cb,
                                                  void*           user);

/* Pull model: copy the next event into *out_evt.
 * Returns MOCKINGBIRD_ERR_QUEUE_EMPTY when nothing is pending (non-blocking). */
MOCKINGBIRD_API mockingbird_status_t mockingbird_poll_event(mockingbird_engine_t*  engine,
                                          mockingbird_event_t*   out_evt);

/* Wake model: lets a host that pulls skip its polling timer. cb fires when
 * an event or packet is queued, then stays quiet until the host calls
 * mockingbird_ack_wake, so a burst of frames costs one call. The host acks
 * first and drains both queues after, which loses no wake: anything queued
 * after the ack fires cb again.
 *
 * cb runs on whichever engine thread queued the entry, under a lock, so it
 * must only signal (post a message, set a flag) and never call into the
 * engine. Once mockingbird_set_wake_callback(engine, NULL, NULL) returns, cb is
 * not running and will not run again, so its resources can be freed.
 * Registering resets the pending state; drain once after registering to pick
 * up anything already queued. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_set_wake_callback(mockingbird_engine_t* engine,
                                                 mockingbird_wake_cb   cb,
                                                 void*                 user);

/* Re-arms the wake callback. Call it before draining. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_ack_wake(mockingbird_engine_t* engine);

/* ============================================================================
 * SEPARATED SEND PATH
 *
 * The data pointer is borrowed. The host must copy/forward then call
 * mockingbird_release with the same packet struct. Holding more than
 * packet_queue_capacity packets at once will exhaust the pool and force
 * overruns on the producing side.
 * ========================================================================== */

/* Pop the next encoded packet. Returns MOCKINGBIRD_ERR_QUEUE_EMPTY if none.
 * out_pkt.data points into the engine pool. Treat as immutable. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_drain(mockingbird_engine_t*  engine,
                                     mockingbird_packet_t*  out_pkt);

/* Return a previously drained packet to the pool. The same struct (with the
 * same .data pointer) must be passed back. Passing a foreign pointer is
 * undefined: returns MOCKINGBIRD_ERR_INVALID_ARG when detectable. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_release(mockingbird_engine_t*        engine,
                                       const mockingbird_packet_t*  pkt);

/* Drain the continuous PCM tap (requires config.enable_pcm_tap).
 *
 * Copies up to max_samples int16 mono samples into out and writes the count to
 * out_written. Unlike mockingbird_drain, this is NOT gated by segments: it delivers
 * every frame the worker processed, so a host can render a waveform or archive
 * the full stream while still receiving detections and encoded packets.
 *
 * Returns MOCKINGBIRD_OK (out_written may be 0 when nothing is buffered),
 * MOCKINGBIRD_ERR_UNSUPPORTED if the tap is disabled, or MOCKINGBIRD_ERR_INVALID_ARG on a
 * null argument. The host owns `out`; nothing is borrowed, so there is no
 * release call. Drain from ONE thread (same single-consumer contract as
 * mockingbird_drain). */
MOCKINGBIRD_API mockingbird_status_t mockingbird_read_pcm(mockingbird_engine_t* engine,
                                        int16_t*        out,
                                        uint32_t        max_samples,
                                        uint32_t*       out_written);

/* ============================================================================
 * RUNTIME RECONFIGURATION
 * ========================================================================== */

/* Retune a RUNNING (or stopped) engine in place, without destroying it.
 *
 * Pass a full mockingbird_config_t: the STRUCTURAL fields must match the ones the
 * engine was created with, and only the TUNABLE subset is applied:
 *
 *   tunable    enable_vad, enable_anomaly, enable_sound_class,
 *              vad_aggressiveness, vad_neural_threshold, vad_hangover_ms,
 *              anomaly_rms_z, anomaly_flux_z, sound_class_threshold
 *
 *   structural everything else (backend, rate, channels, frame_ms, codec,
 *              vad_tier, model paths, rnnoise, roll/segment/background/interval
 *              durations, queue sizes, wait handle): these size buffers, pick
 *              backends, and load models, so they are fixed for the engine's
 *              lifetime.
 *
 * The easiest correct call is: take the config you passed to mockingbird_create,
 * change a tunable field, and pass it back.
 *
 * Returns MOCKINGBIRD_ERR_INVALID_ARG if the config fails normal validation, if any
 * structural field differs, or if it tries to switch ON a detector that was OFF
 * at create time (that detector was never constructed, so it cannot start).
 * Turning a detector off and back on again is supported.
 *
 * The change is applied by the worker between frames: detector state (VAD
 * hysteresis, the anomaly rolling background, the classifier's EMA) is
 * PRESERVED, so retuning mid-utterance does not restart detection or reload
 * models. Safe to call from any thread; it does not block the worker. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_update_config(mockingbird_engine_t*       engine,
                                             const mockingbird_config_t* cfg);

/* ============================================================================
 * OBSERVABILITY
 * ========================================================================== */

MOCKINGBIRD_API mockingbird_status_t mockingbird_get_stats(mockingbird_engine_t*  engine,
                                         mockingbird_stats_t*   out_stats);

/* ============================================================================
 * OPTIONAL WAIT HANDLE
 *
 * Reserved for a pollable handle that lets the host block instead of
 * polling mockingbird_drain. No platform implements it yet: the call
 * validates its arguments and returns MOCKINGBIRD_ERR_UNSUPPORTED. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_wait_handle(mockingbird_engine_t*  engine,
                                           intptr_t*        out_handle);

/* ============================================================================
 * INPUT DEVICES
 *
 * mockingbird_list_input_devices fills up to `capacity` entries for a capture
 * backend and sets *out_count to the number of devices, which can be larger.
 * Pass capacity 0 to count. Ids are opaque, UTF-8 and stable while the device
 * is connected. MINIAUDIO and COREAUDIO on macOS list devices; iOS lists the
 * session's available inputs. Android returns MOCKINGBIRD_ERR_UNSUPPORTED:
 * there, list devices with AudioManager and pass AudioDeviceInfo.getId() as
 * the id.
 *
 * mockingbird_set_input_device selects the device the next start opens;
 * NULL or "" follows the system default, which is also the default. It
 * fails with MOCKINGBIRD_ERR_INVALID_STATE while the engine runs and with
 * MOCKINGBIRD_ERR_UNSUPPORTED for HOST_PCM. The id is copied.
 * ========================================================================== */
typedef struct mockingbird_device {
    char      id[256];
    char      name[256];
    int       is_default;
    uint32_t  _reserved[3];
} mockingbird_device_t;

MOCKINGBIRD_API mockingbird_status_t mockingbird_list_input_devices(mockingbird_backend_t backend,
                                                  mockingbird_device_t* out,
                                                  uint32_t              capacity,
                                                  uint32_t*             out_count);

MOCKINGBIRD_API mockingbird_status_t mockingbird_set_input_device(mockingbird_engine_t* engine,
                                                const char*           device_id);

/* ============================================================================
 * HOST PCM PUSH  (only when backend == MOCKINGBIRD_BACKEND_HOST_PCM)
 *
 * Lets a host with its own capture path feed PCM directly. Returns
 * MOCKINGBIRD_ERR_INVALID_STATE if the engine was created with a different backend.
 *
 * The samples buffer is borrowed for the duration of the call. The engine
 * copies into its ring before returning. capture_ts_us is the monotonic
 * timestamp of the FIRST sample in the buffer.
 *
 * Returns MOCKINGBIRD_ERR_NO_MEMORY equivalent (logged as ring overflow in stats)
 * if the SPSC ring is full; the engine drops the oldest frames silently
 * to keep capture lock-free. */
MOCKINGBIRD_API mockingbird_status_t mockingbird_push_pcm(mockingbird_engine_t*  engine,
                                        const int16_t*   samples,
                                        uint32_t         frame_count,
                                        uint64_t         capture_ts_us);

/* ============================================================================
 * UTILITIES  (pure, thread-safe, no allocation)
 * ========================================================================== */

/* Human-readable status string. Owned by the library; do not free.
 * Accepts any int so hostile callers can pass out-of-range values without
 * triggering enum-domain UB; returns "MOCKINGBIRD_UNKNOWN" for unrecognised codes. */
MOCKINGBIRD_API const char* mockingbird_status_str(int s);

/* "X.Y.Z" of the linked library at runtime. Owned by the library. */
MOCKINGBIRD_API const char* mockingbird_version_string(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MOCKINGBIRD_H */
