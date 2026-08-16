// Auris demo — Start / Stop the mic capture engine, watch counters tick,
// and stream events in a console-style log.

import 'dart:async';
import 'dart:collection';
import 'dart:io' show Platform;
import 'dart:ui' show FontFeature;

import 'package:auris/auris.dart';
import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';

void main() => runApp(const AurisDemoApp());

class AurisDemoApp extends StatelessWidget {
  const AurisDemoApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Auris Demo',
      theme: ThemeData(
        colorSchemeSeed: Colors.indigo,
        useMaterial3: true,
        brightness: Brightness.dark,
      ),
      home: const _HomePage(),
    );
  }
}

/// A named bundle of detector settings — shows how the *same* engine adapts to
/// very different jobs from one config. The sensitivity slider still fine-tunes
/// whichever preset is active.
class _Preset {
  const _Preset(
    this.label,
    this.icon, {
    this.sensitivity = 0.6,
    this.neural = true,
    this.vad = true,
    this.anomaly = true,
    this.soundClass = true,
    this.denoise = false,
  });
  final String label;
  final IconData icon;
  final double sensitivity;
  final bool neural, vad, anomaly, soundClass, denoise;
}

const _presets = <_Preset>[
  _Preset('Balanced', Icons.tune), // everything on, neural VAD
  _Preset('Voice only', Icons.record_voice_over,
      anomaly: false, soundClass: false, denoise: true),
  _Preset('Everything', Icons.hearing, sensitivity: 0.75),
  _Preset('Anomaly watch', Icons.warning_amber_rounded,
      vad: false, soundClass: false, sensitivity: 0.8),
  _Preset('Sound ID', Icons.graphic_eq, vad: false, anomaly: false),
];

class _HomePage extends StatefulWidget {
  const _HomePage();

  @override
  State<_HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<_HomePage> {
  AurisEngine? _engine;
  StreamSubscription<AurisStatsSnapshot>? _statsSub;
  StreamSubscription<AurisEventRecord>? _eventsSub;
  StreamSubscription<AurisPacket>? _packetsSub;
  List<String> _yamnetLabels = const [];

  AurisStatsSnapshot? _stats;
  // Encoded audio actually delivered to the host via the packets stream.
  int _packetCount = 0;
  int _packetBytes = 0;
  final Queue<String> _log = ListQueue<String>();
  String? _error;

  // Single 0..1 sensitivity dial driving every detector via
  // AurisConfig.sensitive. 0 = conservative, 1 = catches everything.
  double _sensitivity = 0.6;
  bool _denoise = false;
  int _presetIndex = 0;

  // Multilingual human-voice state, driven by Silero VAD SPEECH_BEGIN/END
  // (language-agnostic). True between begin and end.
  bool _voiceActive = false;

  // Latest YAMNet recognition (any of the 521 AudioSet classes).
  String _lastSound = '—';
  AurisSoundCategory _lastCategory = AurisSoundCategory.other;
  double _lastSoundConf = 0;

  static const _logMaxLines = 200;

  bool get _isRunning => _engine?.isRunning ?? false;

  @override
  void dispose() {
    _statsSub?.cancel();
    _eventsSub?.cancel();
    _packetsSub?.cancel();
    _engine?.dispose();
    super.dispose();
  }

  Future<void> _start() async {
    setState(() {
      _error = null;
      _packetCount = 0;
      _packetBytes = 0;
    });

    // Mic permission. On iOS/Android permission_handler unifies the request
    // flow (Info.plist / AndroidManifest carry the usage strings). On macOS
    // the plugin isn't registered — the OS TCC prompt fires automatically when
    // CoreAudio capture starts, gated by the app's audio-input entitlement +
    // NSMicrophoneUsageDescription, so we skip the explicit request there.
    if (!Platform.isMacOS) {
      try {
        final status = await Permission.microphone.request();
        if (!status.isGranted) {
          // iOS/Android only surface the system prompt once. Once the user has
          // denied it (or it's restricted by policy), request() returns
          // permanentlyDenied/restricted WITHOUT re-prompting — the only way
          // back is the OS Settings app. Detect that and jump straight there.
          if (status.isPermanentlyDenied || status.isRestricted) {
            setState(() => _error =
                'Microphone access is turned off. Opening Settings — enable '
                'the microphone for this app, then press Start again.');
            await openAppSettings();
          } else {
            setState(() => _error =
                'Microphone permission ${status.name}. Press Start to try again.');
          }
          return;
        }
      } catch (_) {
        // No permission_handler implementation on this platform; fall through
        // and let the OS prompt at capture start.
      }
    }

    // Materialize the Silero VAD and YAMNet sound-classifier models from
    // the bundled Flutter assets to on-disk paths the native engine can
    // open. Both calls are idempotent + cached after first use.
    final sileroPath = await Auris.extractBundledNeuralVadModel();
    final yamnetPath = await Auris.extractBundledSoundClassModel();
    _yamnetLabels = await Auris.loadYamnetLabels();

    try {
      // Each platform now opts into its native backend: CoreAudio on
      // Apple (sub-frame OS host-time timestamps via AudioTimeStamp), Oboe
      // on Android (Google's low-latency wrapper over AAudio / OpenSL ES).
      auris_backend backend;
      if (Platform.isIOS || Platform.isMacOS) {
        backend = auris_backend.AURIS_BACKEND_COREAUDIO;
      } else if (Platform.isAndroid) {
        backend = auris_backend.AURIS_BACKEND_OBOE;
      } else {
        backend = auris_backend.AURIS_BACKEND_MINIAUDIO;
      }

      // The active preset picks WHICH detectors run; the sensitivity slider
      // tunes HOW eager they are. NEURAL VAD (Silero) is language-agnostic, so
      // voice detection is multilingual regardless of the dial.
      final p = _presets[_presetIndex];
      final engine = AurisEngine.create(
        AurisConfig.sensitive(
          _sensitivity,
          backend: backend,
          vadTier: p.neural
              ? auris_vad_tier.AURIS_VAD_NEURAL
              : auris_vad_tier.AURIS_VAD_FAST,
          enableVad: p.vad,
          enableAnomaly: p.anomaly,
          enableSoundClass: p.soundClass,
          enableRnnoise: p.denoise || _denoise,
          neuralVadModelPath: sileroPath,
          soundClassModelPath: yamnetPath,
        ),
      );

      _eventsSub = engine.events.listen((e) {
        setState(() {
          _onEvent(e);
          _log.addFirst(_formatEvent(e));
          while (_log.length > _logMaxLines) {
            _log.removeLast();
          }
        });
      });
      _statsSub =
          engine.stats.listen((s) => setState(() => _stats = s));
      // Encoded audio actually shipped to the host (the send path). In a real
      // app you'd forward p.data; here we just tally what arrived.
      _packetsSub = engine.packets.listen((p) {
        setState(() {
          _packetCount++;
          _packetBytes += p.data.length;
        });
      });

      engine.start();
      setState(() => _engine = engine);
    } on AurisException catch (e) {
      setState(() => _error = '$e');
      await _engine?.dispose();
      _engine = null;
    } catch (e) {
      setState(() => _error = '$e');
      await _engine?.dispose();
      _engine = null;
    }
  }

  /// Update the live indicators from an event. Voice state is driven by
  /// Silero VAD (SPEECH_BEGIN/END) which is multilingual; the YAMNet
  /// classifier feeds the "latest sound" panel.
  void _onEvent(AurisEventRecord e) {
    switch (e.type) {
      case auris_event_type.AURIS_EVT_SPEECH_BEGIN:
        _voiceActive = true;
      case auris_event_type.AURIS_EVT_SPEECH_END:
        _voiceActive = false;
      case auris_event_type.AURIS_EVT_SOUND_CLASS:
        final idx = e.reasonMask;
        _lastCategory = Auris.categoryForYamnetClass(idx);
        _lastSound = (idx >= 0 && idx < _yamnetLabels.length)
            ? _yamnetLabels[idx]
            : 'class $idx';
        _lastSoundConf = e.confidence;
        // YAMNet also confirms voice in any language: if it classifies a
        // human-voice class, treat voice as active even if Silero hasn't
        // fired yet (belt-and-suspenders multilingual gate).
        if (Auris.isHumanVoice(idx)) _voiceActive = true;
      default:
        break;
    }
  }

  String _formatEvent(AurisEventRecord e) {
    if (e.type == auris_event_type.AURIS_EVT_SOUND_CLASS) {
      final idx = e.reasonMask;
      final label = (idx >= 0 && idx < _yamnetLabels.length)
          ? _yamnetLabels[idx]
          : 'class $idx';
      // Show the broad category. AudioSet is English-biased, so non-English
      // speech often lands on "Conversation"/"Narration" rather than the
      // literal "Speech" class — both map to AurisSoundCategory.speech, so
      // the [SPEECH] tag fires regardless of language.
      final cat = Auris.categoryForYamnetClass(idx).name.toUpperCase();
      return '[${e.timestampUs} us] SOUND_CLASS [$cat] "$label" '
          'conf=${e.confidence.toStringAsFixed(2)}';
    }
    return e.toString();
  }

  Future<void> _stop() async {
    final eng = _engine;
    if (eng == null) return;
    await _statsSub?.cancel();
    await _eventsSub?.cancel();
    await _packetsSub?.cancel();
    _statsSub = null;
    _eventsSub = null;
    _packetsSub = null;
    eng.stop();
    await eng.dispose();
    setState(() {
      _engine = null;
      _voiceActive = false;
      _lastSound = '—';
      _lastCategory = AurisSoundCategory.other;
      _lastSoundConf = 0;
    });
  }

  /// Apply a new sensitivity. While running this is a LIVE retune via
  /// `updateConfig` — no teardown, no model reload, no gap in capture, and the
  /// detectors keep their state (an utterance in progress isn't cut off).
  Future<void> _setSensitivity(double v) async {
    setState(() => _sensitivity = v);
    final eng = _engine;
    if (eng == null || !_isRunning) return;
    final p = _presets[_presetIndex];
    try {
      eng.updateConfig(
        AurisConfig.sensitive(
          v,
          backend: eng.config.backend,
          vadTier: eng.config.vadTier,
          enableVad: p.vad,
          enableAnomaly: p.anomaly,
          enableSoundClass: p.soundClass,
          enableRnnoise: eng.config.enableRnnoise,
          neuralVadModelPath: eng.config.neuralVadModelPath,
          soundClassModelPath: eng.config.soundClassModelPath,
        ),
      );
    } on AurisException catch (e) {
      setState(() => _error = 'retune failed: $e');
    }
  }

  /// Switch to a different detector recipe; seeds the slider with the preset's
  /// suggested sensitivity and recreates the engine if running.
  Future<void> _selectPreset(int i) async {
    setState(() {
      _presetIndex = i;
      _sensitivity = _presets[i].sensitivity;
    });
    if (_isRunning) {
      await _stop();
      await _start();
    }
  }

  /// Toggle RNNoise noise suppression; recreates the engine if running.
  Future<void> _setDenoise(bool v) async {
    setState(() => _denoise = v);
    if (_isRunning) {
      await _stop();
      await _start();
    }
  }

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Auris demo'),
        backgroundColor: cs.surfaceContainerHigh,
      ),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  Expanded(
                    child: FilledButton.icon(
                      onPressed: _isRunning ? null : _start,
                      icon: const Icon(Icons.mic),
                      label: const Text('Start'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _isRunning ? _stop : null,
                      icon: const Icon(Icons.stop),
                      label: const Text('Stop'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              // Live input level, straight from stats — works even when no
              // segment is open (packets only flow during segments).
              _LevelMeter(
                rmsDbfs: _stats?.inputRmsDbfs ?? 0,
                peakDbfs: _stats?.inputPeakDbfs ?? 0,
                running: _isRunning,
              ),
              const SizedBox(height: 12),
              // Presets — the same engine, very different jobs. Tapping one
              // recreates the engine with that detector combination.
              Wrap(
                spacing: 8,
                runSpacing: 4,
                children: [
                  for (var i = 0; i < _presets.length; i++)
                    ChoiceChip(
                      avatar: Icon(_presets[i].icon, size: 18),
                      label: Text(_presets[i].label),
                      selected: _presetIndex == i,
                      onSelected: (_) => _selectPreset(i),
                    ),
                ],
              ),
              const SizedBox(height: 12),
              // Sensitivity dial — one knob tunes the active preset's detectors.
              Row(
                children: [
                  const Icon(Icons.tune, size: 18),
                  const SizedBox(width: 8),
                  Text('Sensitivity ${(_sensitivity * 100).round()}%'),
                  Expanded(
                    child: Slider(
                      value: _sensitivity,
                      onChanged: (v) => setState(() => _sensitivity = v),
                      onChangeEnd: _setSensitivity,
                    ),
                  ),
                ],
              ),
              // RNNoise toggle — cleans the signal before every detector.
              SwitchListTile(
                contentPadding: EdgeInsets.zero,
                dense: true,
                secondary: const Icon(Icons.noise_aware),
                title: const Text('Noise suppression (RNNoise)'),
                subtitle: const Text(
                    'Denoise mic before VAD / anomaly / classification'),
                value: _denoise,
                onChanged: _setDenoise,
              ),
              const SizedBox(height: 4),
              if (_error != null)
                Card(
                  color: cs.errorContainer,
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: Text(
                      _error!,
                      style: TextStyle(color: cs.onErrorContainer),
                    ),
                  ),
                ),
              // Big multilingual voice indicator (driven by Silero VAD).
              _VoiceIndicator(active: _voiceActive, running: _isRunning),
              const SizedBox(height: 8),
              // Latest recognized sound (any of 521 AudioSet classes).
              _LastSoundCard(
                label: _lastSound,
                category: _lastCategory,
                confidence: _lastSoundConf,
                running: _isRunning,
              ),
              const SizedBox(height: 12),
              _StatsCard(
                stats: _stats,
                running: _isRunning,
                packetsReceived: _packetCount,
                bytesReceived: _packetBytes,
              ),
              const SizedBox(height: 16),
              Text(
                'Events',
                style: Theme.of(context).textTheme.titleMedium,
              ),
              const SizedBox(height: 8),
              Expanded(
                child: _EventLog(lines: _log.toList()),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

/// Big, language-agnostic "human voice" lamp. Lit while Silero VAD reports
/// speech (any language) or YAMNet classifies a human-voice class.
/// Live input-level meter driven by [AurisStatsSnapshot.inputRmsDbfs]. Works
/// regardless of segmentation — this is exactly what the signal metrics exist
/// for, since encoded packets only flow while a detector holds a segment open.
class _LevelMeter extends StatelessWidget {
  const _LevelMeter({
    required this.rmsDbfs,
    required this.peakDbfs,
    required this.running,
  });
  final double rmsDbfs;
  final double peakDbfs;
  final bool running;

  // Map -60..0 dBFS onto 0..1 for display.
  static double _norm(double dbfs) => ((dbfs + 60) / 60).clamp(0.0, 1.0);

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final level = running ? _norm(rmsDbfs) : 0.0;
    final peak = running ? _norm(peakDbfs) : 0.0;
    final hot = peakDbfs > -3;
    return Row(
      children: [
        Icon(Icons.graphic_eq, size: 18, color: cs.onSurfaceVariant),
        const SizedBox(width: 8),
        Expanded(
          child: Stack(
            alignment: Alignment.centerLeft,
            children: [
              ClipRRect(
                borderRadius: BorderRadius.circular(4),
                child: LinearProgressIndicator(
                  value: level,
                  minHeight: 10,
                  backgroundColor: cs.surfaceContainerHighest,
                  color: hot ? cs.error : cs.primary,
                ),
              ),
              // Peak tick.
              if (running)
                FractionallySizedBox(
                  widthFactor: peak,
                  child: Align(
                    alignment: Alignment.centerRight,
                    child: Container(width: 2, height: 14, color: cs.onSurface),
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(width: 8),
        SizedBox(
          width: 64,
          child: Text(
            running ? '${rmsDbfs.toStringAsFixed(1)} dB' : '—',
            style: Theme.of(context).textTheme.labelSmall,
            textAlign: TextAlign.right,
          ),
        ),
      ],
    );
  }
}

class _VoiceIndicator extends StatelessWidget {
  const _VoiceIndicator({required this.active, required this.running});
  final bool active;
  final bool running;

  @override
  Widget build(BuildContext context) {
    final on = running && active;
    final color = on ? Colors.greenAccent : Colors.grey;
    return Card(
      color: on ? Colors.green.withValues(alpha: 0.18) : null,
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 12),
        child: Row(
          children: [
            Icon(on ? Icons.record_voice_over : Icons.voice_over_off,
                color: color, size: 32),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    on ? 'HUMAN VOICE' : 'No voice',
                    style: TextStyle(
                      fontSize: 20,
                      fontWeight: FontWeight.bold,
                      color: on ? Colors.greenAccent : Colors.grey,
                    ),
                  ),
                  const Text(
                    'Multilingual (Silero VAD) — any language',
                    style: TextStyle(fontSize: 12, color: Colors.grey),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

/// Latest recognized sound across all 521 AudioSet classes, with its broad
/// category colour-coded.
class _LastSoundCard extends StatelessWidget {
  const _LastSoundCard({
    required this.label,
    required this.category,
    required this.confidence,
    required this.running,
  });
  final String label;
  final AurisSoundCategory category;
  final double confidence;
  final bool running;

  Color get _color => switch (category) {
        AurisSoundCategory.speech => Colors.lightBlueAccent,
        AurisSoundCategory.humanOther => Colors.cyanAccent,
        AurisSoundCategory.animal => Colors.orangeAccent,
        AurisSoundCategory.music => Colors.purpleAccent,
        AurisSoundCategory.nature => Colors.tealAccent,
        AurisSoundCategory.other => Colors.grey,
      };

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Row(
          children: [
            Container(
              width: 10,
              height: 36,
              decoration: BoxDecoration(
                color: running ? _color : Colors.grey,
                borderRadius: BorderRadius.circular(4),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    running ? label : '—',
                    style: const TextStyle(
                        fontSize: 18, fontWeight: FontWeight.w600),
                  ),
                  Text(
                    running
                        ? '${category.name} · ${(confidence * 100).round()}%'
                        : 'Latest recognized sound',
                    style: const TextStyle(fontSize: 12, color: Colors.grey),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _StatsCard extends StatelessWidget {
  const _StatsCard({
    required this.stats,
    required this.running,
    required this.packetsReceived,
    required this.bytesReceived,
  });
  final AurisStatsSnapshot? stats;
  final bool running;
  final int packetsReceived;
  final int bytesReceived;

  @override
  Widget build(BuildContext context) {
    final s = stats;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(
                  running ? Icons.fiber_manual_record : Icons.fiber_manual_record_outlined,
                  color: running ? Colors.redAccent : Colors.grey,
                  size: 12,
                ),
                const SizedBox(width: 8),
                Text(
                  running ? 'Capturing' : 'Idle',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ],
            ),
            const SizedBox(height: 12),
            if (s == null)
              const Text('No stats yet. Press Start.')
            else ...[
              _statRow('frames_captured', s.framesCaptured),
              _statRow('frames_dropped_capture', s.framesDroppedCapture),
              _statRow('packets_emitted', s.packetsEmitted),
              _statRow('packets_dropped', s.packetsDropped),
              _statRow('packets_received (host)', packetsReceived),
              _statRow('bytes_received (host)', bytesReceived),
              _statRow('segments_opened', s.segmentsOpened),
              _statRow('segments_closed', s.segmentsClosed),
              _statRow('pcm_ring_fill_frames', s.pcmRingFillFrames),
              _statRow('packet_queue_fill', s.packetQueueFill),
            ],
          ],
        ),
      ),
    );
  }

  Widget _statRow(String name, int value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        children: [
          Expanded(child: Text(name)),
          Text(
            value.toString(),
            style: const TextStyle(fontFeatures: [FontFeature.tabularFigures()]),
          ),
        ],
      ),
    );
  }
}

class _EventLog extends StatelessWidget {
  const _EventLog({required this.lines});
  final List<String> lines;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: lines.isEmpty
          ? const Center(child: Text('No events yet.'))
          : ListView.builder(
              padding: const EdgeInsets.all(8),
              itemCount: lines.length,
              itemBuilder: (context, i) => Text(
                lines[i],
                style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
              ),
            ),
    );
  }
}
