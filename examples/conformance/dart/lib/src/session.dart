import 'dart:io';

import 'package:mockingbird_conformance/src/byte_reader.dart';
import 'package:mockingbird_conformance/src/engine_run.dart';
import 'package:mockingbird_conformance/src/format.dart';
import 'package:mockingbird_conformance/src/signal.dart';
import 'package:mockingbird_conformance/src/wire.dart';

/// One exchange with the listening peer: swap signals, process each other's,
/// swap digests. True when both directions match.
Future<bool> runSession(
  EngineRun run,
  String host,
  int port,
  String peerName,
) async {
  const name = 'dart';
  final socket = await Socket.connect(host, port);
  socket.setOption(SocketOption.tcpNoDelay, true);
  final reader = ByteReader(socket);
  final own = genSignal(dartSeed);
  final expected = run.process(own);
  stdout.writeln(
    '[$name] generated ${own.length} samples (seed ${hex16(dartSeed)}); '
    'my engine: ${expected.nPackets} packets / ${expected.nEvents} events / '
    '${expected.nBytes} bytes, digest ${hex16(expected.digest)}',
  );
  writeFrame(socket, pcmToBytes(own));
  final peerBytes = await readFrame(reader);
  if (!bytesEqual(peerBytes, pcmToBytes(genSignal(goSeed)))) {
    stdout.writeln(
      '[$name] GENERATOR MISMATCH: $peerName signal differs from our local '
      'regeneration',
    );
    await socket.close();
    return false;
  }
  final peerPcm = bytesToPcm(peerBytes);
  stdout.writeln(
    '[$name] received ${peerPcm.length} samples from $peerName; '
    'generator parity MATCH, processing through MY binding...',
  );
  final computed = run.process(peerPcm);
  stdout.writeln(
    "[$name] my result for $peerName's audio: ${computed.nPackets} packets / "
    '${computed.nEvents} events / ${computed.nBytes} bytes, digest '
    '${hex16(computed.digest)}',
  );
  writeFrame(
    socket,
    encodeResult(
      computed.digest,
      expected.digest,
      computed.nPackets,
      computed.nEvents,
      computed.nBytes,
    ),
  );
  final rd = (await readFrame(reader)).buffer.asByteData();
  final dirA = rd.getUint64(0) == expected.digest;
  final dirB = computed.digest == rd.getUint64(8);
  stdout.writeln(
    '[$name] parity $name-audio to $peerName: ${ok(dirA)}   |   '
    '$peerName-audio to $name: ${ok(dirB)}',
  );
  await socket.flush();
  await socket.close();
  return dirA && dirB;
}
