import 'dart:ffi';

import 'package:mockingbird_conformance/src/dump.dart';
import 'package:mockingbird_conformance/src/engine_queues.dart';
import 'package:mockingbird_conformance/src/hasher.dart';
import 'package:mockingbird_conformance/src/proc_result.dart';
import 'package:mockingbird_platform_interface/ffi.dart';

/// Hashes a stopped engine's output: packets, then events, then four stats
/// counters, the order every peer uses.
ProcResult digestOutput(
  MockingbirdBindings b,
  Pointer<mockingbird_engine_t> engine, {
  required bool dump,
}) {
  final h = Hasher();
  final packets = drainPackets(b, engine);
  var nBytes = 0;
  h.u32(packets.length);
  for (final (m, data) in packets) {
    h
      ..u64(m[0])
      ..u64(m[1])
      ..u64(m[2])
      ..u32(m[3])
      ..u32(m[4])
      ..u32(m[5])
      ..bytes(data);
    nBytes += data.length;
  }
  final events = pollEvents(b, engine);
  h.u32(events.length);
  for (final e in events) {
    h
      ..u32(e[0])
      ..u64(e[1])
      ..u64(e[2])
      ..u32(e[3])
      ..u32(e[4]);
  }
  final s = readStats(b, engine);
  h
    ..u64(s[0])
    ..u64(s[2])
    ..u64(s[4])
    ..u64(s[5]);
  if (dump) printDump(packets, events, s);
  return (
    digest: h.h,
    nPackets: packets.length,
    nEvents: events.length,
    nBytes: nBytes,
  );
}
