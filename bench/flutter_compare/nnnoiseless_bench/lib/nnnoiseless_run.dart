import 'dart:typed_data';

import 'package:flutter_nnnoiseless/flutter_nnnoiseless.dart';
import 'package:harness/harness.dart';

class NnnoiselessRun {
  NnnoiselessRun(this.fixture);

  final Fixture fixture;

  Future<Map<String, num>> execute() async {
    final session = await NoiselessSession.create(
      sampleRate: Fixture.sampleRate,
    );
    var frames = 0;
    var voiced = 0;
    var outBytes = 0;
    for (final chunk in fixture.chunks(320)) {
      final result = await session.process(Uint8List.sublistView(chunk));
      outBytes += result.audio.length;
      if (result.isVoice()) voiced++;
      frames++;
    }
    outBytes += (await session.flush()).length;
    session.dispose();
    return {
      'frames_pushed': frames,
      'voiced_frames': voiced,
      'samples_out': outBytes ~/ 2,
    };
  }
}
