import 'dart:io';

import 'package:mockingbird_conformance/src/engine_queues.dart';

/// Prints every packet, event and the stats line, for diffing two peers.
void printDump(
  List<DrainedPacket> packets,
  List<List<int>> events,
  List<int> s,
) {
  for (var i = 0; i < packets.length; i++) {
    final p = packets[i].$1;
    stdout.writeln(
      'P ${i.toString().padLeft(2, '0')} size=${p[5]} flags=${p[3]} '
      'seg=${p[0]} pts=${p[1]} cap=${p[2]}',
    );
  }
  for (var i = 0; i < events.length; i++) {
    final e = events[i];
    stdout.writeln(
      'E ${i.toString().padLeft(2, '0')} type=${e[0]} reason=${e[3]} '
      'conf=${e[4].toRadixString(16).padLeft(8, '0')} seg=${e[1]} ts=${e[2]}',
    );
  }
  stdout.writeln(
    'S captured=${s[0]} dropped=${s[1]} emitted=${s[2]} pktDropped=${s[3]} '
    'segOpen=${s[4]} segClose=${s[5]}',
  );
}
