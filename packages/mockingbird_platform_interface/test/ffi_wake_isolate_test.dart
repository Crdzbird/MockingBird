@TestOn('vm')
library;

import 'dart:async';
import 'dart:ffi';
import 'dart:isolate';

import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_wake.dart';

import 'src/host_library.dart';

Object get _skip => HostLibrary.available ? false : 'build the library first';

void _registerAndDie(SendPort reply) {
  final b = HostLibrary.bindings();
  final engine = HostLibrary.startedEngine(b);
  FfiWake(b).set(FfiEngineHandle(engine), () {});
  HostLibrary.push(b, engine, 10);
  Isolate.exit(reply, engine.address);
}

void main() {
  test('wakes reach this isolate and stop once released', () async {
    final b = HostLibrary.bindings();
    final h = FfiEngineHandle(HostLibrary.startedEngine(b));
    final wake = FfiWake(b);
    var wakes = 0;
    final first = Completer<void>();
    expect(
      wake.set(h, () {
        wakes++;
        if (!first.isCompleted) first.complete();
      }),
      isTrue,
    );
    HostLibrary.push(b, h.pointer, 5);
    await first.future.timeout(const Duration(seconds: 5));
    expect(wake.set(h, null), isTrue);
    expect(h.wake, isNull);
    wake.ack(h);
    final seen = wakes;
    HostLibrary.push(b, h.pointer, 5);
    await Future<void>.delayed(const Duration(milliseconds: 100));
    expect(wakes, seen);
    b
      ..mockingbird_stop(h.pointer)
      ..mockingbird_destroy(h.pointer);
    h.free();
  }, skip: _skip);

  test(
    'an isolate that dies with its wake registered brings nothing down',
    () async {
      final reply = ReceivePort();
      await Isolate.spawn(_registerAndDie, reply.sendPort);
      final address = await reply.first as int;
      final b = HostLibrary.bindings();
      final engine = Pointer<mockingbird_engine_t>.fromAddress(address);
      for (var round = 0; round < 20; round++) {
        b.mockingbird_ack_wake(engine);
        HostLibrary.push(b, engine, 5);
        await Future<void>.delayed(const Duration(milliseconds: 5));
      }
      final stats = calloc<mockingbird_stats_t>();
      b
        ..mockingbird_stop(engine)
        ..mockingbird_get_stats(engine, stats);
      expect(stats.ref.packets_emitted, 110);
      calloc.free(stats);
      b.mockingbird_destroy(engine);
    },
    skip: _skip,
  );
}
