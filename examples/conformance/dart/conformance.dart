// Dart peer of the Mockingbird cross-binding conformance harness. Mirror of
// examples/conformance/go/main.go. It drives the SAME native libmockingbird through
// the Dart FFI bindings (the exact code path the Flutter plugin uses), exchanges
// deterministic audio with the Go peer over TCP, and proves both bindings turn
// identical input PCM into byte-identical packets, events, and stats.
//
// Run the Go peer first, then:
//   dart run conformance.dart 127.0.0.1:9999            # auto-locate libmockingbird
//   dart run conformance.dart 127.0.0.1:9999 /path/to/libmockingbird.dylib
//
// The determinism contract (signal generator, config, processing order, and the
// FNV-1a/64 canonical digest) is identical to the Go peer.

import 'dart:async';
import 'dart:collection';
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/ffi.dart';
import 'package:ffi/ffi.dart';

const int sampleRate = 16000;
const int frameLen = 320;
const int numSamples = 32000;
const int goSeed = 0xA1A1A1A1A1A1A1A1;
const int dartSeed = 0xB2B2B2B2B2B2B2B2;

Int16List genSignal(int seed) {
  int x = seed;
  final out = Int16List(numSamples);
  for (int i = 0; i < numSamples; i++) {
    x ^= (x << 13);
    x ^= (x >>> 7);
    x ^= (x << 17);
    int v = ((x >>> 40) & 0x1FFF) - 4096;
    if (i % 8000 < 1600) {
      v += ((i >> 3) & 1) == 0 ? 11000 : -11000;
    }
    if (v > 32767) {
      v = 32767;
    } else if (v < -32768) {
      v = -32768;
    }
    out[i] = v;
  }
  return out;
}

class Hasher {
  int h = 0xcbf29ce484222325;
  static const int _prime = 0x100000001b3;

  void byteVal(int b) {
    h ^= (b & 0xFF);
    h = h * _prime;
  }

  void bytes(Uint8List p) {
    for (final b in p) {
      byteVal(b);
    }
  }

  void u32(int v) {
    byteVal((v >>> 24) & 0xFF);
    byteVal((v >>> 16) & 0xFF);
    byteVal((v >>> 8) & 0xFF);
    byteVal(v & 0xFF);
  }

  void u64(int v) {
    for (int s = 56; s >= 0; s -= 8) {
      byteVal((v >>> s) & 0xFF);
    }
  }
}

int float32bits(double d) {
  final bd = ByteData(4);
  bd.setFloat32(0, d, Endian.little);
  return bd.getUint32(0, Endian.little);
}

class ProcResult {
  final int digest;
  final int nPackets;
  final int nEvents;
  final int nBytes;
  ProcResult(this.digest, this.nPackets, this.nEvents, this.nBytes);
}

ProcResult process(MockingbirdBindings b, Int16List pcm) {
  final cfg = calloc<mockingbird_config_t>();
  final native = calloc<Int16>(pcm.length);
  final outEngine = calloc<Pointer<mockingbird_engine_t>>();
  final pkt = calloc<mockingbird_packet_t>();
  final ev = calloc<mockingbird_event_t>();
  final stats = calloc<mockingbird_stats_t>();
  try {
    if (b.mockingbird_config_defaults(cfg) != mockingbird_status.MOCKINGBIRD_OK) {
      throw StateError('mockingbird_config_defaults failed');
    }
    cfg.ref.backendAsInt = 4;
    cfg.ref.codecAsInt = 1;
    cfg.ref.pcm_ring_frames = 1 << 17;
    cfg.ref.packet_queue_capacity = 4096;

    if (b.mockingbird_create(cfg, outEngine) != mockingbird_status.MOCKINGBIRD_OK) {
      throw StateError('mockingbird_create failed');
    }
    final engine = outEngine.value;

    if (b.mockingbird_start(engine) != mockingbird_status.MOCKINGBIRD_OK) {
      b.mockingbird_destroy(engine);
      throw StateError('mockingbird_start failed');
    }

    native.asTypedList(pcm.length).setAll(0, pcm);
    final nFrames = pcm.length ~/ frameLen;
    for (int f = 0; f < nFrames; f++) {
      final ptr = Pointer<Int16>.fromAddress(native.address + f * frameLen * 2);
      if (b.mockingbird_push_pcm(engine, ptr, frameLen, f * 20000) !=
          mockingbird_status.MOCKINGBIRD_OK) {
        throw StateError('mockingbird_push_pcm failed');
      }
    }

    b.mockingbird_stop(engine);

    final h = Hasher();
    final packets = <List<int>>[];
    final datas = <Uint8List>[];
    while (b.mockingbird_drain(engine, pkt) == mockingbird_status.MOCKINGBIRD_OK) {
      final r = pkt.ref;
      final data = Uint8List.fromList(r.data.asTypedList(r.size));
      packets.add([r.segment_id, r.pts_us, r.capture_ts_us, r.flags, r.codecAsInt, r.size]);
      datas.add(data);
      b.mockingbird_release(engine, pkt);
    }
    int nBytes = 0;
    h.u32(packets.length);
    for (int i = 0; i < packets.length; i++) {
      final p = packets[i];
      h.u64(p[0]);
      h.u64(p[1]);
      h.u64(p[2]);
      h.u32(p[3]);
      h.u32(p[4]);
      h.u32(p[5]);
      h.bytes(datas[i]);
      nBytes += datas[i].length;
    }

    final events = <List<int>>[];
    while (b.mockingbird_poll_event(engine, ev) == mockingbird_status.MOCKINGBIRD_OK) {
      final r = ev.ref;
      events.add([r.typeAsInt, r.segment_id, r.timestamp_us, r.reason_mask, float32bits(r.confidence)]);
    }
    h.u32(events.length);
    for (final e in events) {
      h.u32(e[0]);
      h.u64(e[1]);
      h.u64(e[2]);
      h.u32(e[3]);
      h.u32(e[4]);
    }

    b.mockingbird_get_stats(engine, stats);
    final s = stats.ref;
    h.u64(s.frames_captured);
    h.u64(s.packets_emitted);
    h.u64(s.segments_opened);
    h.u64(s.segments_closed);

    if (debug) {
      for (int i = 0; i < packets.length; i++) {
        final p = packets[i];
        stdout.writeln('P ${i.toString().padLeft(2, '0')} size=${p[5]} flags=${p[3]} '
            'seg=${p[0]} pts=${p[1]} cap=${p[2]}');
      }
      for (int i = 0; i < events.length; i++) {
        final e = events[i];
        stdout.writeln('E ${i.toString().padLeft(2, '0')} type=${e[0]} reason=${e[3]} '
            'conf=${e[4].toRadixString(16).padLeft(8, '0')} seg=${e[1]} ts=${e[2]}');
      }
      stdout.writeln('S captured=${s.frames_captured} dropped=${s.frames_dropped_capture} '
          'emitted=${s.packets_emitted} pktDropped=${s.packets_dropped} '
          'segOpen=${s.segments_opened} segClose=${s.segments_closed}');
    }

    b.mockingbird_destroy(engine);
    return ProcResult(h.h, packets.length, events.length, nBytes);
  } finally {
    calloc.free(cfg);
    calloc.free(native);
    calloc.free(outEngine);
    calloc.free(pkt);
    calloc.free(ev);
    calloc.free(stats);
  }
}

class ByteReader {
  final Queue<int> _buf = Queue<int>();
  final Queue<_Req> _reqs = Queue<_Req>();
  ByteReader(Socket s) {
    s.listen((d) {
      _buf.addAll(d);
      _pump();
    });
  }
  Future<Uint8List> read(int n) {
    final c = Completer<Uint8List>();
    _reqs.add(_Req(n, c));
    _pump();
    return c.future;
  }

  void _pump() {
    while (_reqs.isNotEmpty && _buf.length >= _reqs.first.n) {
      final req = _reqs.removeFirst();
      final out = Uint8List(req.n);
      for (int i = 0; i < req.n; i++) {
        out[i] = _buf.removeFirst();
      }
      req.completer.complete(out);
    }
  }
}

class _Req {
  final int n;
  final Completer<Uint8List> completer;
  _Req(this.n, this.completer);
}

void writeFrame(Socket s, Uint8List payload) {
  final hdr = ByteData(4)..setUint32(0, payload.length, Endian.big);
  s.add(hdr.buffer.asUint8List());
  s.add(payload);
}

Future<Uint8List> readFrame(ByteReader r) async {
  final hdr = await r.read(4);
  final n = hdr.buffer.asByteData().getUint32(0, Endian.big);
  return r.read(n);
}

Uint8List pcmToBytes(Int16List s) {
  final bd = ByteData(s.length * 2);
  for (int i = 0; i < s.length; i++) {
    bd.setInt16(i * 2, s[i], Endian.little);
  }
  return bd.buffer.asUint8List();
}

Int16List bytesToPcm(Uint8List b) {
  final out = Int16List(b.length ~/ 2);
  final bd = b.buffer.asByteData(b.offsetInBytes);
  for (int i = 0; i < out.length; i++) {
    out[i] = bd.getInt16(i * 2, Endian.little);
  }
  return out;
}

Uint8List encodeResult(int computedDigest, int expectedDigest, int nPkts, int nEvents, int nBytes) {
  final bd = ByteData(28);
  bd.setUint64(0, computedDigest, Endian.big);
  bd.setUint64(8, expectedDigest, Endian.big);
  bd.setUint32(16, nPkts, Endian.big);
  bd.setUint32(20, nEvents, Endian.big);
  bd.setUint32(24, nBytes, Endian.big);
  return bd.buffer.asUint8List();
}

bool bytesEqual(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (int i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

String hex16(int v) => '0x${(v & 0xFFFFFFFFFFFFFFFF).toRadixString(16).padLeft(16, '0')}';
String ok(bool b) => b ? 'MATCH' : 'MISMATCH';

String resolveLib(List<String> args) {
  final positional = args.where((a) => !a.startsWith('--')).toList();
  if (positional.length >= 2) return positional[1];
  final ext = Platform.isMacOS ? 'dylib' : (Platform.isWindows ? 'dll' : 'so');
  final base = Platform.script.resolve('../../../build/');
  for (final name in ['libmockingbird.$ext', 'libmockingbird.0.1.0.$ext']) {
    final p = base.resolve(name).toFilePath();
    if (File(p).existsSync()) return p;
  }
  return base.resolve('libmockingbird.$ext').toFilePath();
}

bool debug = false;

Future<void> main(List<String> args) async {
  if (args.isEmpty) {
    stderr.writeln('usage: dart run conformance.dart host:port [libmockingbird-path]');
    exit(2);
  }
  if (args[0] == 'dump') {
    debug = true;
    final libPath = args.length >= 2 ? args[1] : resolveLib(['dump']);
    final b = MockingbirdBindings(DynamicLibrary.open(libPath));
    process(b, genSignal(goSeed));
    return;
  }
  final hp = args[0].split(':');
  final host = hp[0];
  final port = int.parse(hp[1]);

  final libPath = resolveLib(args);
  final b = MockingbirdBindings(DynamicLibrary.open(libPath));

  final socket = await Socket.connect(host, port);
  socket.setOption(SocketOption.tcpNoDelay, true);
  final reader = ByteReader(socket);

  const name = 'dart';
  final peerName = args
      .firstWhere((a) => a.startsWith('--peer='), orElse: () => '--peer=go')
      .substring('--peer='.length);
  final own = genSignal(dartSeed);
  final expected = process(b, own);
  stdout.writeln('[$name] generated ${own.length} samples (seed ${hex16(dartSeed)}); '
      'my engine: ${expected.nPackets} packets / ${expected.nEvents} events / ${expected.nBytes} bytes, '
      'digest ${hex16(expected.digest)}');

  writeFrame(socket, pcmToBytes(own));
  final peerBytes = await readFrame(reader);

  if (!bytesEqual(peerBytes, pcmToBytes(genSignal(goSeed)))) {
    stdout.writeln('[$name] GENERATOR MISMATCH: $peerName signal differs from our local regeneration');
    await socket.close();
    exit(1);
  }
  final peerPcm = bytesToPcm(peerBytes);
  stdout.writeln('[$name] received ${peerPcm.length} samples from $peerName; '
      'generator parity MATCH, processing through MY binding...');

  final computed = process(b, peerPcm);
  stdout.writeln('[$name] my result for $peerName\'s audio: ${computed.nPackets} packets / '
      '${computed.nEvents} events / ${computed.nBytes} bytes, digest ${hex16(computed.digest)}');

  writeFrame(socket, encodeResult(computed.digest, expected.digest, computed.nPackets, computed.nEvents, computed.nBytes));
  final rb = await readFrame(reader);
  final rd = rb.buffer.asByteData();
  final peerComputedForMine = rd.getUint64(0, Endian.big);
  final peerExpectedForTheirs = rd.getUint64(8, Endian.big);

  final dirA = peerComputedForMine == expected.digest;
  final dirB = computed.digest == peerExpectedForTheirs;

  stdout.writeln('[$name] parity $name-audio to $peerName: ${ok(dirA)}   |   '
      '$peerName-audio to $name: ${ok(dirB)}');

  await socket.flush();
  await socket.close();

  if (dirA && dirB) {
    stdout.writeln('\nCONFORMANCE PASS: Dart and $peerName produce '
        'byte-identical results both ways');
  } else {
    stdout.writeln('\nCONFORMANCE FAIL: divergence detected');
    exit(1);
  }
}
