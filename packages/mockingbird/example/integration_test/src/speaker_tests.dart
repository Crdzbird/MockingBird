import 'dart:typed_data';

import 'package:flutter/services.dart' show rootBundle;
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

import 'host_audio.dart';

Future<Int16List> _clip(String name) async {
  final data = await rootBundle.load('assets/$name');
  return Int16List.fromList(
    data.buffer.asInt16List(
      data.offsetInBytes + 44,
      (data.lengthInBytes - 44) ~/ 2,
    ),
  );
}

/// Speaker verification downloads its model and tells two voices apart.
void speakerTests() {
  test('a voice print matches its speaker and not another', () async {
    final engine = MockingbirdEngine.create(
      hostConfig(vad: const MockingbirdVadConfig(aggressiveness: 3)),
    )..setSpeakerModel(await MockingbirdModels.speakerModelPath());
    addTearDown(engine.dispose);
    final a = await _clip('speaker_a1.wav');
    final b = await _clip('speaker_b.wav');
    final target = engine.speakerEmbedding(a.sublist(0, 16000 * 3));
    expect(target, hasLength(192));
    engine.setTargetSpeaker(target);
    final verdicts = <MockingbirdSpeakerEvent>[];
    addTearDown(
      engine.events.listen((e) {
        if (e case final MockingbirdSpeakerEvent v) verdicts.add(v);
      }).cancel,
    );
    engine.start();
    var ts = 0;
    for (final clip in [
      a.sublist(16000 * 2),
      Int16List(32000),
      b,
      Int16List(32000),
    ]) {
      for (var i = 0; i + 320 <= clip.length; i += 320, ts += 20000) {
        engine.pushPcm(
          Int16List.sublistView(clip, i, i + 320),
          captureTsUs: ts,
        );
        if (i % 6400 == 0) await Future<void>.delayed(Duration.zero);
      }
    }
    await Future<void>.delayed(const Duration(seconds: 1));
    expect(verdicts.length, greaterThanOrEqualTo(2));
    expect(verdicts.first.isTarget, isTrue);
    expect(verdicts.last.isTarget, isFalse);
  });
}
