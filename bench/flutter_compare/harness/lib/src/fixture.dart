import 'dart:typed_data';

import 'package:harness/src/bench_paths.dart';
import 'package:harness/src/wav_reader.dart';

class Fixture {
  Fixture._(this.samples);

  factory Fixture.standard(BenchPaths paths, {int targetSeconds = 60}) {
    const reader = WavReader();
    final clips = [
      for (final name in ['hello.wav', 'music.wav', 'miaow_16k.wav'])
        reader.read(paths.fixture(name)),
    ];
    final gap = Int16List(sampleRate);
    final out = BytesBuilder(copy: false);
    var total = 0;
    while (total < targetSeconds * sampleRate) {
      for (final clip in clips) {
        out.add(Uint8List.sublistView(clip));
        out.add(Uint8List.sublistView(gap));
        total += clip.length + gap.length;
      }
    }
    final bytes = out.takeBytes();
    return Fixture._(Int16List.sublistView(bytes));
  }

  static const int sampleRate = 16000;

  final Int16List samples;

  double get seconds => samples.length / sampleRate;

  Float32List toFloat32() {
    final out = Float32List(samples.length);
    for (var i = 0; i < samples.length; i++) {
      out[i] = samples[i] / 32768.0;
    }
    return out;
  }

  Iterable<Int16List> chunks(int n) sync* {
    for (var i = 0; i + n <= samples.length; i += n) {
      yield Int16List.sublistView(samples, i, i + n);
    }
  }

  Iterable<Float32List> floatChunks(int n) sync* {
    final all = toFloat32();
    for (var i = 0; i + n <= all.length; i += n) {
      yield Float32List.sublistView(all, i, i + n);
    }
  }
}
