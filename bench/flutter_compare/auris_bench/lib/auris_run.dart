import 'package:auris/auris.dart';
import 'package:harness/harness.dart';

class AurisRun {
  AurisRun(this.config, this.fixture);

  final AurisConfig config;
  final Fixture fixture;

  Future<Map<String, num>> execute() async {
    final engine = AurisEngine.create(config)..start();
    var packets = 0;
    var speech = 0;
    var segments = 0;
    var sounds = 0;
    AurisStats? last;
    final subscriptions = [
      engine.packets.listen((_) => packets++),
      engine.events.listen((event) {
        switch (event) {
          case AurisSpeechBeginEvent():
            speech++;
          case AurisSegmentBeginEvent():
            segments++;
          case AurisSoundClassEvent():
            sounds++;
          default:
            break;
        }
      }),
      engine.stats.listen((stats) => last = stats),
    ];
    var pushed = 0;
    var ts = 0;
    for (final chunk in fixture.chunks(320)) {
      engine.pushPcm(chunk, captureTsUs: ts);
      ts += 20000;
      pushed++;
      if (pushed % 100 == 0) {
        await Future<void>.delayed(const Duration(milliseconds: 100));
      }
    }
    while ((last?.pcmRingFillFrames ?? 1) > 0) {
      await Future<void>.delayed(const Duration(milliseconds: 50));
    }
    engine.stop();
    await Future<void>.delayed(const Duration(milliseconds: 120));
    for (final s in subscriptions) {
      await s.cancel();
    }
    await engine.dispose();
    return {
      'frames_pushed': pushed,
      'packets': packets,
      'speech_begin': speech,
      'segments': segments,
      'sound_class_events': sounds,
      'frames_dropped_capture': last?.framesDroppedCapture ?? -1,
    };
  }
}
