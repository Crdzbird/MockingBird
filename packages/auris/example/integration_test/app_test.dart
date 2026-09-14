// On-device end-to-end verification of the Auris plugin.
//
// This runs on a real device / simulator through the real plugin, so it proves
// the whole chain: the native engine is linked into the app, its C ABI is
// reachable from dart:ffi (DynamicLibrary.process()), and the ONNX-backed
// detectors actually load — which is exactly what a Swift Package Manager build
// has to get right.
//
//   flutter test integration_test -d macos
//   flutter test integration_test -d <simulator-id>
//
// Everything uses the HOST_PCM backend and synthetic audio, so no microphone
// permission is needed and the results are deterministic.

import 'dart:math';
import 'dart:typed_data';

import 'package:auris/auris.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

/// 20 ms of 16 kHz audio: a loud square wave (something for the detectors to
/// chew on) or silence.
Int16List frame({int amplitude = 0}) {
  final f = Int16List(320);
  for (var i = 0; i < f.length; i++) {
    f[i] = (i % 32 < 16) ? amplitude : -amplitude;
  }
  return f;
}

AurisConfig hostConfig({
  bool vad = false,
  bool anomaly = false,
  bool tap = false,
  auris_vad_tier tier = auris_vad_tier.AURIS_VAD_FAST,
  String? neuralModel,
  bool soundClass = false,
  String? soundModel,
}) =>
    AurisConfig(
      backend: auris_backend.AURIS_BACKEND_HOST_PCM,
      codec: auris_codec.AURIS_CODEC_PCM_S16LE,
      enableVad: vad,
      vadTier: tier,
      neuralVadModelPath: neuralModel,
      enableAnomaly: anomaly,
      enableSoundClass: soundClass,
      soundClassModelPath: soundModel,
      enablePcmTap: tap,
      pcmTapFrames: 1 << 15,
      pcmRingFrames: 1 << 17,
      packetQueueCapacity: 4096,
    );

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  group('native ABI reachable via dart:ffi', () {
    test('version string resolves', () {
      // If the plugin were mislinked, or the C symbols were not exported into
      // the app's dynamic symbol table, this throws "symbol not found".
      final v = Auris.version();
      expect(v, isNotEmpty);
      expect(RegExp(r'^\d+\.\d+\.\d+').hasMatch(v), isTrue,
          reason: 'unexpected version string: $v');
    });
  });

  group('engine end to end (HOST_PCM, no microphone)', () {
    test('passthrough produces one packet per pushed frame', () async {
      final engine = AurisEngine.create(hostConfig())..start();
      addTearDown(engine.dispose);

      final packets = <AurisPacket>[];
      final sub = engine.packets.listen(packets.add);
      addTearDown(sub.cancel);

      for (var i = 0; i < 10; i++) {
        engine.pushPcm(frame(), captureTsUs: i * 20000);
      }
      await Future<void>.delayed(const Duration(milliseconds: 400));

      expect(packets.length, 10);
      expect(packets.fold<int>(0, (n, p) => n + p.data.length), 10 * 320 * 2);
    });

    test('stats report input level, and the PCM tap is ungated', () async {
      final engine = AurisEngine.create(hostConfig(tap: true))..start();
      addTearDown(engine.dispose);

      const amp = 8192;
      for (var i = 0; i < 10; i++) {
        engine.pushPcm(frame(amplitude: amp), captureTsUs: i * 20000);
      }
      await Future<void>.delayed(const Duration(milliseconds: 400));

      // Level metering: a square wave at amp has RMS == amp exactly.
      final stats = await engine.stats.first;
      final want = 20 * (log(amp / 32768.0) / ln10);
      expect(stats.inputRmsDbfs, closeTo(want, 0.5));

      // The tap carries every processed sample, regardless of segmentation.
      var tapped = 0;
      while (true) {
        final chunk = engine.readPcm(maxSamples: 4096);
        if (chunk.isEmpty) break;
        tapped += chunk.length;
      }
      expect(tapped, 10 * 320);
    });

    test('live retune applies without restarting the engine', () async {
      final cfg = hostConfig(anomaly: true);
      final engine = AurisEngine.create(cfg)..start();
      addTearDown(engine.dispose);

      for (var i = 0; i < 5; i++) {
        engine.pushPcm(frame(amplitude: 4000), captureTsUs: i * 20000);
      }
      await Future<void>.delayed(const Duration(milliseconds: 200));

      // Tunable field: accepted in place.
      engine.updateConfig(hostConfig(anomaly: true));
      expect(engine.isRunning, isTrue);

      // Structural field: rejected rather than silently ignored.
      expect(
        () => engine.updateConfig(
          AurisConfig(
            backend: auris_backend.AURIS_BACKEND_HOST_PCM,
            sampleRateHz: 48000,
            enableAnomaly: true,
          ),
        ),
        throwsA(isA<AurisException>()),
      );
    });
  });

  group('worker health and silence gate', () {
    test('stats expose worker health; the gate sheds YAMNet on silence',
        () async {
      final model = await Auris.extractBundledSoundClassModel();
      final engine = AurisEngine.create(
        hostConfig(soundClass: true, soundModel: model),
      )..start();
      addTearDown(engine.dispose);

      // 5 s of digital silence: YAMNet inferences come due but every window
      // is entirely quiet, so they are shed rather than run.
      for (var i = 0; i < 250; i++) {
        engine.pushPcm(frame(), captureTsUs: i * 20000);
        if (i % 25 == 24) {
          await Future<void>.delayed(const Duration(milliseconds: 20));
        }
      }
      await Future<void>.delayed(const Duration(milliseconds: 500));
      var stats = await engine.stats.first;
      expect(stats.soundClassInferences, 0);
      expect(stats.soundClassInferencesSkipped, greaterThan(0));
      expect(stats.workerWakeups, greaterThan(0));
      expect(stats.degradationLevel, 0); // adaptive is off by default

      // Then real signal: inference resumes.
      for (var i = 250; i < 350; i++) {
        engine.pushPcm(frame(amplitude: 9000), captureTsUs: i * 20000);
        if (i % 25 == 24) {
          await Future<void>.delayed(const Duration(milliseconds: 20));
        }
      }
      await Future<void>.delayed(const Duration(milliseconds: 800));
      stats = await engine.stats.first;
      expect(stats.soundClassInferences, greaterThan(0));
    });
  });

  group('ONNX-backed detectors load (proves ONNX Runtime is linked)', () {
    test('neural (Silero) VAD initialises and detects a loud signal', () async {
      final model = await Auris.extractBundledNeuralVadModel();
      expect(model, isNotEmpty);

      // If ONNX Runtime were missing, auris_create would reject NEURAL with
      // AURIS_ERR_UNSUPPORTED and this would throw.
      final engine = AurisEngine.create(
        hostConfig(
          vad: true,
          tier: auris_vad_tier.AURIS_VAD_NEURAL,
          neuralModel: model,
        ),
      )..start();
      addTearDown(engine.dispose);

      // Feed enough audio for Silero to run several 512-sample windows.
      for (var i = 0; i < 60; i++) {
        engine.pushPcm(frame(amplitude: 12000), captureTsUs: i * 20000);
        if (i % 10 == 9) {
          await Future<void>.delayed(const Duration(milliseconds: 40));
        }
      }
      await Future<void>.delayed(const Duration(milliseconds: 600));

      final stats = await engine.stats.first;
      expect(stats.framesCaptured, 60 * 320);
      // Reaching here means the model loaded and inference ran per frame
      // without the detector failing out.
      expect(engine.isRunning, isTrue);
    });

    test('YAMNet classifier initialises and emits a sound class', () async {
      final model = await Auris.extractBundledSoundClassModel();
      expect(model, isNotEmpty);

      final engine = AurisEngine.create(
        hostConfig(soundClass: true, soundModel: model),
      )..start();
      addTearDown(engine.dispose);

      final classes = <int>[];
      final sub = engine.events
          .where((e) => e.type == auris_event_type.AURIS_EVT_SOUND_CLASS)
          .listen((e) => classes.add(e.reasonMask));
      addTearDown(sub.cancel);

      // YAMNet needs ~2 s of audio before its first inference.
      for (var i = 0; i < 150; i++) {
        engine.pushPcm(frame(amplitude: 9000), captureTsUs: i * 20000);
        if (i % 10 == 9) {
          await Future<void>.delayed(const Duration(milliseconds: 30));
        }
      }
      await Future<void>.delayed(const Duration(seconds: 1));

      expect(classes, isNotEmpty,
          reason: 'YAMNet produced no classification — is ONNX Runtime linked?');
      // Every class index must be a valid AudioSet id.
      for (final c in classes) {
        expect(c, inInclusiveRange(0, 520));
      }
    });
  });
}
