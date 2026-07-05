// Auris — high-level Dart API.
//
// Wraps the raw FFI bindings (exposed by auris_platform_interface) with a
// disposable Engine class that owns the native handle, streams events and
// stats, and counts packets drained off the queue.
//
// Events, packets, and stats are all drained from the engine's owned queues
// on a periodic poll timer. The C ABI also exposes a push callback
// (auris_set_event_callback), but a Dart NativeCallable.listener fires
// asynchronously after the native call returns, so the borrowed event
// pointer would dangle — polling the owned pull queue is the correct path
// for a GC'd runtime. See _drainEvents.

import 'dart:async';
import 'dart:ffi' as ffi;
import 'dart:io' show File;
import 'dart:typed_data';

import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart' show rootBundle;
import 'package:path_provider/path_provider.dart';

export 'package:auris_platform_interface/auris_platform_interface.dart'
    show
        AurisBindings,
        auris_backend,
        auris_codec,
        auris_event_type,
        auris_sample_format,
        auris_status,
        auris_vad_tier;

/// Snapshot of engine counters at a moment in time.
class AurisStatsSnapshot {
  const AurisStatsSnapshot({
    required this.framesCaptured,
    required this.framesDroppedCapture,
    required this.packetsEmitted,
    required this.packetsDropped,
    required this.segmentsOpened,
    required this.segmentsClosed,
    required this.pcmRingFillFrames,
    required this.packetQueueFill,
  });

  final int framesCaptured;
  final int framesDroppedCapture;
  final int packetsEmitted;
  final int packetsDropped;
  final int segmentsOpened;
  final int segmentsClosed;
  final int pcmRingFillFrames;
  final int packetQueueFill;
}

/// One event observed by the worker thread.
class AurisEventRecord {
  const AurisEventRecord({
    required this.type,
    required this.segmentId,
    required this.timestampUs,
    required this.reasonMask,
    required this.confidence,
  });

  final auris_event_type type;
  final int segmentId;
  final int timestampUs;
  final int reasonMask;
  final double confidence;

  @override
  String toString() =>
      '[$timestampUs us] ${type.name} segment=$segmentId reason=$reasonMask '
      'conf=${confidence.toStringAsFixed(2)}';
}

/// One encoded audio packet drained from the engine. [data] is a Dart-owned
/// copy of the bytes — the native zero-copy borrow is released immediately
/// after the copy, so the packet is safe to keep and forward.
class AurisPacket {
  const AurisPacket({
    required this.segmentId,
    required this.ptsUs,
    required this.captureTsUs,
    required this.flags,
    required this.codec,
    required this.data,
  });

  final int segmentId;
  final int ptsUs;
  final int captureTsUs;
  final int flags;
  final auris_codec codec;
  final Uint8List data;

  @override
  String toString() =>
      'AurisPacket(segment=$segmentId pts=$ptsUs ${codec.name} ${data.length}B)';
}

/// Configuration shape mirrored from auris_config_t. Every detector knob is
/// surfaced so callers can tune sensitivity per detector or, more simply,
/// via [AurisConfig.sensitive] for a single 0..1 dial that drives all of
/// them at once.
class AurisConfig {
  const AurisConfig({
    this.backend = auris_backend.AURIS_BACKEND_AUTO,
    this.sampleRateHz = 16000,
    this.frameMs = 20,
    this.codec = auris_codec.AURIS_CODEC_PCM_S16LE,
    // --- Preprocess ---
    this.enableRnnoise = false,
    // --- VAD (human-voice detection, multilingual via Silero) ---
    this.enableVad = true,
    this.vadTier = auris_vad_tier.AURIS_VAD_FAST,
    this.vadAggressiveness = 2,
    this.vadNeuralThreshold = 0.5,
    this.vadHangoverMs = 300,
    this.neuralVadModelPath,
    // --- Anomaly gate ("weird sound" detection) ---
    this.enableAnomaly = false,
    this.anomalyRmsZ = 3.0,
    this.anomalyFluxZ = 3.0,
    this.anomalyBackgroundMs = 2000,
    // --- YAMNet sound classification (every recognizable sound) ---
    this.enableSoundClass = false,
    this.soundClassModelPath,
    this.soundClassThreshold = 0.4,
    this.soundClassIntervalMs = 500,
    // --- Segmentation + encoder + queues (parity with the C ABI / Go) ---
    this.preRollMs = 300,
    this.postRollMs = 300,
    this.segmentMaxMs = 60000,
    this.opusBitrateBps = 32000,
    this.opusVbr = true,
    this.opusComplexity = 5,
    this.pcmRingFrames = 16384,
    this.packetQueueCapacity = 256,
  });

  /// Build a config from a single [sensitivity] dial in 0..1, where 0 is the
  /// least sensitive (fewest detections, fewest false positives) and 1 is the
  /// most sensitive (catches faint / borderline sounds, more false positives).
  ///
  /// The dial maps to all four detectors at once:
  ///   - Silero VAD P(speech) threshold:   0.80 .. 0.20
  ///   - libfvad aggressiveness:            3    .. 0
  ///   - anomaly RMS / flux z-score:        5.0  .. 1.5
  ///   - YAMNet top-1 probability:          0.70 .. 0.12
  ///
  /// Voice detection stays multilingual regardless of [sensitivity]: Silero
  /// VAD is language-agnostic by construction; the dial only changes how
  /// eagerly it calls a frame "speech".
  factory AurisConfig.sensitive(
    double sensitivity, {
    auris_backend backend = auris_backend.AURIS_BACKEND_AUTO,
    int sampleRateHz = 16000,
    int frameMs = 20,
    auris_codec codec = auris_codec.AURIS_CODEC_PCM_S16LE,
    auris_vad_tier vadTier = auris_vad_tier.AURIS_VAD_NEURAL,
    bool enableVad = true,
    bool enableAnomaly = true,
    bool enableSoundClass = true,
    bool enableRnnoise = false,
    String? neuralVadModelPath,
    String? soundClassModelPath,
    int soundClassIntervalMs = 500,
  }) {
    final s = sensitivity.clamp(0.0, 1.0);
    double lerp(double a, double b) => a + (b - a) * s;
    return AurisConfig(
      backend: backend,
      sampleRateHz: sampleRateHz,
      frameMs: frameMs,
      codec: codec,
      enableRnnoise: enableRnnoise,
      enableVad: enableVad,
      vadTier: vadTier,
      vadAggressiveness: (lerp(3, 0)).round().clamp(0, 3),
      vadNeuralThreshold: lerp(0.80, 0.20),
      neuralVadModelPath: neuralVadModelPath,
      enableAnomaly: enableAnomaly,
      anomalyRmsZ: lerp(5.0, 1.5),
      anomalyFluxZ: lerp(5.0, 1.5),
      enableSoundClass: enableSoundClass,
      soundClassModelPath: soundClassModelPath,
      soundClassThreshold: lerp(0.70, 0.12),
      soundClassIntervalMs: soundClassIntervalMs,
    );
  }

  final auris_backend backend;
  final int sampleRateHz;
  final int frameMs;
  final auris_codec codec;

  // --- Preprocess ----------------------------------------------------------

  /// Enable RNNoise recurrent noise suppression ahead of every detector and
  /// the encoder, so VAD / anomaly / classification and the emitted audio all
  /// see the cleaned signal. Off by default (adds a little CPU + ~1 ms
  /// latency). Requires an integer 48 kHz ratio: [sampleRateHz] must be one of
  /// 8000, 16000, or 48000 (32000 is rejected by the engine).
  final bool enableRnnoise;

  // --- VAD -----------------------------------------------------------------

  /// Enable voice activity detection (human-voice onset/offset). Drives the
  /// language-agnostic SPEECH_BEGIN / SPEECH_END events.
  final bool enableVad;

  /// FAST (libfvad) or NEURAL (Silero). NEURAL is the more robust multilingual
  /// detector; FAST is zero-dependency. Both are language-agnostic.
  final auris_vad_tier vadTier;

  /// libfvad aggressiveness 0..3. Higher = more eager to reject borderline
  /// frames as non-speech. Ignored for NEURAL.
  final int vadAggressiveness;

  /// Silero P(speech) threshold 0..1. Lower = more sensitive (catches quieter
  /// / borderline speech). Ignored for FAST.
  final double vadNeuralThreshold;

  /// Trailing silence (ms) before a speech segment closes.
  final int vadHangoverMs;

  /// Filesystem path to the Silero ONNX model. Only consulted when
  /// [vadTier] is [auris_vad_tier.AURIS_VAD_NEURAL]. Use
  /// [Auris.extractBundledNeuralVadModel] to materialize the asset-bundled
  /// copy into an app-writable location.
  final String? neuralVadModelPath;

  // --- Anomaly -------------------------------------------------------------

  /// Enable the adaptive anomaly gate (RMS + spectral flux + centroid over a
  /// rolling background). Fires on sudden / unusual sounds.
  final bool enableAnomaly;

  /// RMS z-score trigger threshold. Lower = more sensitive.
  final double anomalyRmsZ;

  /// Spectral-flux z-score trigger threshold. Lower = more sensitive.
  final double anomalyFluxZ;

  /// Rolling background window (ms) the z-scores are measured against.
  final int anomalyBackgroundMs;

  // --- Sound classification ------------------------------------------------

  /// Enable YAMNet semantic sound classification. When true, the engine
  /// runs YAMNet every [soundClassIntervalMs] over a 3 s rolling window
  /// and emits [AURIS_EVT_SOUND_CLASS] events whose reason_mask is the
  /// AudioSet class index (0..520) — covering all 521 recognizable classes:
  /// speech, animals, music, alarms, glass, vehicles, nature, and more.
  final bool enableSoundClass;

  /// Path to the YAMNet ONNX model. Use
  /// [Auris.extractBundledSoundClassModel] to materialize the bundled copy.
  final String? soundClassModelPath;

  /// Top-1 probability threshold for emitting a sound-class event.
  /// 0..1; lower = more sensitive. 0 falls back to 0.4.
  final double soundClassThreshold;

  /// Inference cadence in milliseconds. Smaller = more responsive but more
  /// CPU. 0 falls back to 500.
  final int soundClassIntervalMs;

  /// Audio buffered before a segment trigger, emitted as PREROLL packets (ms).
  final int preRollMs;

  /// Audio kept after a segment closes, emitted as POSTROLL packets (ms).
  final int postRollMs;

  /// Hard cap on a single segment's length (ms); a longer segment is split.
  final int segmentMaxMs;

  /// Opus target bitrate (bps). Only used when [codec] is
  /// [auris_codec.AURIS_CODEC_OPUS].
  final int opusBitrateBps;

  /// Opus variable-bitrate toggle.
  final bool opusVbr;

  /// Opus complexity 0..10 (higher = better quality, more CPU).
  final int opusComplexity;

  /// Capacity of the capture→worker PCM ring, in samples.
  final int pcmRingFrames;

  /// Capacity of the worker→host packet queue (drop-oldest when full).
  final int packetQueueCapacity;
}

/// Broad semantic category derived from a YAMNet AudioSet class index.
///
/// YAMNet emits one of 521 fine-grained AudioSet classes, but those split a
/// single perceptual category across many labels. Crucially for speech, the
/// human-voice cluster (classes 0..12) covers Speech, Conversation,
/// Narration, Babbling, Shout, Yell, Whispering, etc. AudioSet is
/// English/Western-biased, so non-English speech frequently lands on
/// "Conversation" (2) or "Narration" (3) rather than the literal "Speech"
/// (0). Mapping to [AurisSoundCategory.speech] makes language-agnostic
/// "is this voice?" decisions robust.
enum AurisSoundCategory {
  /// Human speech in any language (AudioSet 0..12).
  speech,

  /// Other human vocal/body sounds: laughter, crying, singing, breathing,
  /// footsteps, applause, etc. (AudioSet 13..66).
  humanOther,

  /// Animal sounds: dogs, cats, birds, livestock, etc. (AudioSet 67..131).
  animal,

  /// Music and musical instruments (AudioSet 132..282).
  music,

  /// Natural ambient sounds: wind, water, thunder, fire (AudioSet 283..299).
  nature,

  /// Everything else: vehicles, alarms, tools, glass, explosions, plus the
  /// "Silence" class (494).
  other,
}

/// Static helpers tied to the plugin's bundled assets.
class Auris {
  Auris._();

  /// Map a YAMNet AudioSet class index to a broad [AurisSoundCategory].
  ///
  /// Boundaries follow the AudioSet ontology top-level groups as laid out
  /// in `yamnet_class_map.txt`:
  ///   0..12   speech (human voice)
  ///   13..66  other human sounds
  ///   67..131 animal
  ///   132..282 music
  ///   283..299 nature
  ///   else    other (vehicles, alarms, things, silence@494, ...)
  static AurisSoundCategory categoryForYamnetClass(int classId) {
    if (classId >= 0 && classId <= 12) return AurisSoundCategory.speech;
    if (classId >= 13 && classId <= 66) return AurisSoundCategory.humanOther;
    if (classId >= 67 && classId <= 131) return AurisSoundCategory.animal;
    if (classId >= 132 && classId <= 282) return AurisSoundCategory.music;
    if (classId >= 283 && classId <= 299) return AurisSoundCategory.nature;
    return AurisSoundCategory.other;
  }

  /// True if the YAMNet class is human speech in any language. Use this for
  /// language-agnostic speech detection from the classifier; the Silero
  /// neural VAD (SPEECH_BEGIN/END events) is also language-agnostic and
  /// lower-latency.
  static bool isSpeechClass(int classId) =>
      categoryForYamnetClass(classId) == AurisSoundCategory.speech;

  /// True if the YAMNet class is any human vocalization in any language:
  /// speech (0..12), laughter (13..18), crying (19..23), and singing /
  /// humming / chanting / groaning (24..34). This is the broadest
  /// "is this a human voice?" test and is fully multilingual — it keys off
  /// the AudioSet human-voice branch, not any language model.
  ///
  /// Excludes non-vocal human sounds (whistling, breathing, footsteps,
  /// clapping, heartbeat), which are produced by a person but not the voice.
  static bool isHumanVoice(int classId) => classId >= 0 && classId <= 34;

  /// Asset key for the Silero VAD model shipped inside the auris plugin.
  static const String kBundledNeuralVadAsset =
      'packages/auris/assets/silero_vad.onnx';

  /// Asset key for the YAMNet sound classifier model.
  static const String kBundledYamnetAsset =
      'packages/auris/assets/yamnet_3s.onnx';

  /// Asset key for YAMNet's 521-class AudioSet label map (text, one
  /// `<index> <label>` per line).
  static const String kBundledYamnetClassMapAsset =
      'packages/auris/assets/yamnet_class_map.txt';

  static String? _cachedSileroPath;
  static String? _cachedYamnetPath;
  static List<String>? _cachedYamnetLabels;

  /// Copy the bundled Silero VAD model out of the Flutter asset bundle
  /// (which is not on the filesystem) into an app-private directory where
  /// the native engine can `fopen` it. Idempotent + caches the path.
  ///
  /// Returns the on-disk path. Call once before constructing an engine
  /// with [auris_vad_tier.AURIS_VAD_NEURAL].
  static Future<String> extractBundledNeuralVadModel() async {
    _cachedSileroPath ??= await _extract(kBundledNeuralVadAsset, 'silero_vad.onnx');
    return _cachedSileroPath!;
  }

  /// Materialize the bundled YAMNet sound classifier model on disk.
  /// Pass the returned path to [AurisConfig.soundClassModelPath].
  static Future<String> extractBundledSoundClassModel() async {
    _cachedYamnetPath ??= await _extract(kBundledYamnetAsset, 'yamnet_3s.onnx');
    return _cachedYamnetPath!;
  }

  /// Returns YAMNet's AudioSet class labels (index 0..520) loaded from
  /// the bundled `yamnet_class_map.txt`. Cached after the first call.
  /// Useful for turning [AURIS_EVT_SOUND_CLASS] reason_mask values into
  /// human-readable names (e.g. "Speech", "Dog", "Glass", "Smash, crash").
  static Future<List<String>> loadYamnetLabels() async {
    if (_cachedYamnetLabels != null) return _cachedYamnetLabels!;
    final data = await rootBundle.loadString(kBundledYamnetClassMapAsset);
    final labels = <String>[];
    for (final line in data.split('\n')) {
      final t = line.trim();
      if (t.isEmpty) continue;
      // Format: "<index> <label>".
      final sp = t.indexOf(' ');
      labels.add(sp < 0 ? t : t.substring(sp + 1));
    }
    _cachedYamnetLabels = labels;
    return labels;
  }

  static Future<String> _extract(String asset, String filename) async {
    final dir = await getApplicationSupportDirectory();
    final out = File('${dir.path}/$filename');
    if (!out.existsSync() || out.lengthSync() == 0) {
      final data = await rootBundle.load(asset);
      await out.writeAsBytes(
        data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes),
        flush: true,
      );
    }
    return out.path;
  }
}

/// Thin owner of a single native engine handle. Call [start], drive events
/// off the streams, then [stop] + [dispose] when done.
class AurisEngine {
  AurisEngine._(this._bindings, this._engine, this._config);

  /// Constructs and immediately calls `auris_create`. Throws [AurisException]
  /// if the native side rejects the config.
  factory AurisEngine.create(AurisConfig config) {
    final bindings = AurisPlatform.instance.bindings;

    final cfgPtr = calloc<auris_config_t>();
    try {
      _checkOk(bindings.auris_config_defaults(cfgPtr), 'auris_config_defaults');

      // ffigen exposes enum-typed C fields as `<name>AsInt` setters with
      // a read-only `<name>` getter that wraps the value in the Dart enum.
      cfgPtr.ref
        ..backendAsInt = config.backend.value
        ..sample_rate_hz = config.sampleRateHz
        ..frame_ms = config.frameMs
        ..codecAsInt = config.codec.value
        // Preprocess
        ..enable_rnnoise = config.enableRnnoise ? 1 : 0
        // VAD
        ..enable_vad = config.enableVad ? 1 : 0
        ..vad_tierAsInt = config.vadTier.value
        ..vad_aggressiveness = config.vadAggressiveness
        ..vad_neural_threshold = config.vadNeuralThreshold
        ..vad_hangover_ms = config.vadHangoverMs
        // Anomaly
        ..enable_anomaly = config.enableAnomaly ? 1 : 0
        ..anomaly_rms_z = config.anomalyRmsZ
        ..anomaly_flux_z = config.anomalyFluxZ
        ..anomaly_background_ms = config.anomalyBackgroundMs
        // Segmentation
        ..pre_roll_ms = config.preRollMs
        ..post_roll_ms = config.postRollMs
        ..segment_max_ms = config.segmentMaxMs
        // Encoder
        ..opus_bitrate_bps = config.opusBitrateBps
        ..opus_vbr = config.opusVbr ? 1 : 0
        ..opus_complexity = config.opusComplexity
        // Queues
        ..pcm_ring_frames = config.pcmRingFrames
        ..packet_queue_capacity = config.packetQueueCapacity;

      // Neural VAD model path. Borrowed by auris_create; the engine copies
      // any string it needs to retain, so freeing right after returns is
      // safe. try/finally so the buffer is freed even on _checkOk throw.
      ffi.Pointer<Utf8>? modelPathPtr;
      if (config.neuralVadModelPath != null &&
          config.neuralVadModelPath!.isNotEmpty) {
        modelPathPtr = config.neuralVadModelPath!.toNativeUtf8();
        cfgPtr.ref.neural_vad_model_path = modelPathPtr.cast<ffi.Char>();
      }

      // Sound classifier knobs.
      ffi.Pointer<Utf8>? soundClassPathPtr;
      cfgPtr.ref.enable_sound_class = config.enableSoundClass ? 1 : 0;
      cfgPtr.ref.sound_class_threshold = config.soundClassThreshold;
      cfgPtr.ref.sound_class_interval_ms = config.soundClassIntervalMs;
      if (config.soundClassModelPath != null &&
          config.soundClassModelPath!.isNotEmpty) {
        soundClassPathPtr = config.soundClassModelPath!.toNativeUtf8();
        cfgPtr.ref.sound_class_model_path =
            soundClassPathPtr.cast<ffi.Char>();
      }

      final outEngine = calloc<ffi.Pointer<auris_engine_t>>();
      try {
        _checkOk(bindings.auris_create(cfgPtr, outEngine), 'auris_create');
        return AurisEngine._(bindings, outEngine.value, config);
      } finally {
        calloc.free(outEngine);
        if (modelPathPtr != null) calloc.free(modelPathPtr);
        if (soundClassPathPtr != null) calloc.free(soundClassPathPtr);
      }
    } finally {
      calloc.free(cfgPtr);
    }
  }

  // // FFI-BOUNDARY  Events are drained from the engine's bounded pull queue
  // via auris_poll_event, NOT via a push callback.
  //
  // Why not NativeCallable.listener? The C callback fires on the worker
  // thread (T2). A NativeCallable.listener marshals that call asynchronously
  // onto the Dart isolate's event loop, so by the time our Dart code runs,
  // the worker's stack-allocated auris_event_t the pointer referenced is
  // long gone. Dereferencing it reads freed/reused memory (observed as
  // garbage segment ids on a busy event stream). The pull queue holds
  // engine-OWNED copies, so polling is the safe, correct path for a GC'd
  // async runtime. Latency is bounded by the poll interval (50 ms default).
  final AurisBindings _bindings;
  ffi.Pointer<auris_engine_t> _engine;
  final AurisConfig _config;

  Timer? _pollTimer;
  bool _running = false;
  bool _disposed = false;

  final _events = StreamController<AurisEventRecord>.broadcast();
  final _packets = StreamController<AurisPacket>.broadcast();
  final _stats = StreamController<AurisStatsSnapshot>.broadcast();

  /// Stream of events observed by the worker thread.
  Stream<AurisEventRecord> get events => _events.stream;

  /// Stream of encoded audio packets (each carries a Dart-owned byte copy).
  Stream<AurisPacket> get packets => _packets.stream;

  /// Stream of stats snapshots taken on the polling interval.
  Stream<AurisStatsSnapshot> get stats => _stats.stream;

  /// The configuration this engine was created with.
  AurisConfig get config => _config;

  /// True once [start] has succeeded and before [stop] has been called.
  bool get isRunning => _running;

  /// Begin capture. Spawns the worker on the native side and a poll timer on
  /// the Dart side.
  ///
  /// [pollInterval] controls how often the engine is drained and events are
  /// pulled. 50 ms is a reasonable default for a UI refreshing at 20 Hz.
  void start({Duration pollInterval = const Duration(milliseconds: 50)}) {
    _ensureLive();
    if (_running) return;
    _checkOk(_bindings.auris_start(_engine), 'auris_start');
    _running = true;
    _pollTimer = Timer.periodic(pollInterval, (_) => _poll());
  }

  /// Stop capture and the poll timer.
  void stop() {
    _ensureLive();
    if (!_running) return;
    _pollTimer?.cancel();
    _pollTimer = null;
    _checkOk(_bindings.auris_stop(_engine), 'auris_stop');
    _running = false;
    // One last drain to capture trailing events (SEGMENT_END is emitted
    // during stop()).
    _poll();
  }

  /// Destroy the native handle. Idempotent. After this call the engine is
  /// unusable; create a new one if needed.
  Future<void> dispose() async {
    if (_disposed) return;
    if (_running) stop();
    _disposed = true;
    _bindings.auris_destroy(_engine);
    _engine = ffi.nullptr;
    await _events.close();
    await _packets.close();
    await _stats.close();
  }

  // --- polling ------------------------------------------------------------

  void _poll() {
    if (_disposed || _engine == ffi.nullptr) return;
    // A native error inside the timer callback would otherwise escape to the
    // isolate's unhandled-error zone — invisible to stream listeners — and the
    // timer would keep re-throwing. Forward it onto the streams instead.
    try {
      _drainEvents();
      _drainPackets();
      _emitStats();
    } catch (e, st) {
      _events.addError(e, st);
    }
  }

  void _drainEvents() {
    final evt = calloc<auris_event_t>();
    try {
      while (true) {
        final s = _bindings.auris_poll_event(_engine, evt);
        if (s == auris_status.AURIS_ERR_QUEUE_EMPTY) return;
        if (s != auris_status.AURIS_OK) {
          throw AurisException('auris_poll_event', s);
        }
        // // FFI-BOUNDARY  auris_poll_event COPIES the event into our buffer,
        // so reading evt.ref here is safe (unlike the async push callback).
        final e = evt.ref;
        _events.add(
          AurisEventRecord(
            type: e.type,
            segmentId: e.segment_id,
            timestampUs: e.timestamp_us,
            reasonMask: e.reason_mask,
            confidence: e.confidence,
          ),
        );
      }
    } finally {
      calloc.free(evt);
    }
  }

  void _drainPackets() {
    final pkt = calloc<auris_packet_t>();
    try {
      while (true) {
        final s = _bindings.auris_drain(_engine, pkt);
        if (s == auris_status.AURIS_ERR_QUEUE_EMPTY) return;
        if (s != auris_status.AURIS_OK) {
          throw AurisException('auris_drain', s);
        }
        // // FFI-BOUNDARY  data is a const borrow into the engine's pool that
        // becomes invalid the instant auris_release returns. Copy it into a
        // Dart-owned Uint8List FIRST, then release, then emit the copy.
        final p = pkt.ref;
        final view = (p.size > 0 && p.data != ffi.nullptr)
            ? p.data.asTypedList(p.size)
            : Uint8List(0);
        final packet = AurisPacket(
          segmentId: p.segment_id,
          ptsUs: p.pts_us,
          captureTsUs: p.capture_ts_us,
          flags: p.flags,
          codec: p.codec,
          data: Uint8List.fromList(view), // copy out of the pool
        );
        final r = _bindings.auris_release(_engine, pkt);
        if (r != auris_status.AURIS_OK) {
          throw AurisException('auris_release', r);
        }
        _packets.add(packet);
      }
    } finally {
      calloc.free(pkt);
    }
  }

  void _emitStats() {
    final s = calloc<auris_stats_t>();
    try {
      final st = _bindings.auris_get_stats(_engine, s);
      if (st != auris_status.AURIS_OK) {
        throw AurisException('auris_get_stats', st);
      }
      final v = s.ref;
      _stats.add(
        AurisStatsSnapshot(
          framesCaptured: v.frames_captured,
          framesDroppedCapture: v.frames_dropped_capture,
          packetsEmitted: v.packets_emitted,
          packetsDropped: v.packets_dropped,
          segmentsOpened: v.segments_opened,
          segmentsClosed: v.segments_closed,
          pcmRingFillFrames: v.pcm_ring_fill_frames,
          packetQueueFill: v.packet_queue_fill,
        ),
      );
    } finally {
      calloc.free(s);
    }
  }

  void _ensureLive() {
    if (_disposed) {
      throw StateError('AurisEngine has been disposed.');
    }
  }
}

/// Thrown when a C ABI call returns a non-OK status.
class AurisException implements Exception {
  AurisException(this.call, this.status);
  final String call;
  final auris_status status;

  @override
  String toString() => 'AurisException: $call returned ${status.name}';
}

void _checkOk(auris_status s, String call) {
  if (s != auris_status.AURIS_OK) throw AurisException(call, s);
}
