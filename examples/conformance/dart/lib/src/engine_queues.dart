import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_conformance/src/format.dart';
import 'package:mockingbird_platform_interface/ffi.dart';

/// One packet's header fields and a copy of its bytes.
typedef DrainedPacket = (List<int> meta, Uint8List data);

/// Drains every packet: segment, pts, capture time, flags, codec, size.
List<DrainedPacket> drainPackets(
  MockingbirdBindings b,
  Pointer<mockingbird_engine_t> engine,
) {
  final pkt = calloc<mockingbird_packet_t>();
  try {
    final out = <DrainedPacket>[];
    while (b.mockingbird_drain(engine, pkt) == .MOCKINGBIRD_OK) {
      final r = pkt.ref;
      final meta = [
        r.segment_id,
        r.pts_us,
        r.capture_ts_us,
        r.flags,
        r.codecAsInt,
        r.size,
      ];
      out.add((meta, Uint8List.fromList(r.data.asTypedList(r.size))));
      b.mockingbird_release(engine, pkt);
    }
    return out;
  } finally {
    calloc.free(pkt);
  }
}

/// Polls every event: type, segment, timestamp, reason, confidence bits.
List<List<int>> pollEvents(
  MockingbirdBindings b,
  Pointer<mockingbird_engine_t> engine,
) {
  final ev = calloc<mockingbird_event_t>();
  try {
    final out = <List<int>>[];
    while (b.mockingbird_poll_event(engine, ev) == .MOCKINGBIRD_OK) {
      final r = ev.ref;
      out.add([
        r.typeAsInt,
        r.segment_id,
        r.timestamp_us,
        r.reason_mask,
        float32bits(r.confidence),
      ]);
    }
    return out;
  } finally {
    calloc.free(ev);
  }
}

/// Captured, dropped, emitted, packets dropped, segments opened and closed.
List<int> readStats(
  MockingbirdBindings b,
  Pointer<mockingbird_engine_t> engine,
) {
  final st = calloc<mockingbird_stats_t>();
  try {
    b.mockingbird_get_stats(engine, st);
    final s = st.ref;
    return [
      s.frames_captured,
      s.frames_dropped_capture,
      s.packets_emitted,
      s.packets_dropped,
      s.segments_opened,
      s.segments_closed,
    ];
  } finally {
    calloc.free(st);
  }
}
