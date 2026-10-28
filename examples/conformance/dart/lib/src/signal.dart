import 'dart:typed_data';

/// Samples per frame at 16 kHz and 20 ms.
const int frameLen = 320;

/// Samples each peer generates.
const int numSamples = 32000;

/// The listener's seed, whatever its language.
const int goSeed = 0xA1A1A1A1A1A1A1A1;

/// The dialer's seed, whatever its language.
const int dartSeed = 0xB2B2B2B2B2B2B2B2;

/// The deterministic test signal: xorshift noise with a square-wave burst
/// in the first fifth of every half second, identical in every peer.
Int16List genSignal(int seed) {
  var x = seed;
  final out = Int16List(numSamples);
  for (var i = 0; i < numSamples; i++) {
    x ^= x << 13;
    x ^= x >>> 7;
    x ^= x << 17;
    var v = ((x >>> 40) & 0x1FFF) - 4096;
    if (i % 8000 < 1600) v += ((i >> 3) & 1) == 0 ? 11000 : -11000;
    out[i] = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
  }
  return out;
}
