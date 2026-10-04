import 'dart:async';
import 'dart:typed_data';

import 'package:harness/harness.dart';
import 'package:vad/vad.dart';

class VadRun {
  VadRun(this.fixture);

  final Fixture fixture;

  Future<Map<String, num>> execute() async {
    final handler = VadHandler.create();
    var frames = 0;
    var starts = 0;
    var ends = 0;
    final subscriptions = [
      handler.onFrameProcessed.listen((_) => frames++),
      handler.onSpeechStart.listen((_) => starts++),
      handler.onSpeechEnd.listen((_) => ends++),
    ];
    final controller = StreamController<Uint8List>();
    await handler.startListening(
      model: 'v5',
      frameSamples: 512,
      audioStream: controller.stream,
    );
    var pushed = 0;
    for (final chunk in fixture.chunks(512)) {
      controller.add(Uint8List.sublistView(chunk));
      pushed++;
      if (pushed % 8 == 0) await Future<void>.delayed(Duration.zero);
    }
    final deadline = DateTime.now().add(const Duration(seconds: 180));
    while (frames < pushed && DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(const Duration(milliseconds: 20));
    }
    await handler.stopListening();
    await controller.close();
    for (final s in subscriptions) {
      await s.cancel();
    }
    await handler.dispose();
    return {
      'frames_pushed': pushed,
      'frames_processed': frames,
      'speech_start': starts,
      'speech_end': ends,
    };
  }
}
