import 'dart:async';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'fake_engine_handle.dart';
import 'zero_stats.dart';

/// Records the engine calls the app makes, including when the wake listener
/// is registered and released, so tests can check their order.
final class FakeEnginePlatform extends MockingbirdPlatform {
  /// Creates the fake.
  factory FakeEnginePlatform() = FakeEnginePlatform._;

  FakeEnginePlatform._();

  /// Every engine call, by name, in order.
  final calls = <String>[];

  /// The status stop returns.
  int stopStatus = MockingbirdStatusCode.ok;

  /// When set, start waits for it, as a microphone does while it opens.
  Completer<void>? opening;

  /// The wake listener currently registered.
  void Function()? wake;

  @override
  MockingbirdCreateResult create(MockingbirdNativeConfig config) {
    calls.add('create');
    return (status: MockingbirdStatusCode.ok, handle: const FakeEngineHandle());
  }

  @override
  Future<int> startAsync(MockingbirdEngineHandle handle) async {
    calls.add('start');
    await opening?.future;
    return MockingbirdStatusCode.ok;
  }

  @override
  Future<int> stopAsync(MockingbirdEngineHandle handle) async {
    calls.add('stop');
    return stopStatus;
  }

  @override
  Future<void> destroyAsync(MockingbirdEngineHandle handle) async =>
      calls.add(wake == null ? 'destroy' : 'destroy with the wake registered');

  @override
  bool setWakeListener(
    MockingbirdEngineHandle handle,
    void Function()? onWake,
  ) {
    calls.add(onWake == null ? 'wake off' : 'wake on');
    wake = onWake;
    return true;
  }

  @override
  void ackWake(MockingbirdEngineHandle handle) {}

  @override
  MockingbirdEventResult pollEvent(MockingbirdEngineHandle handle) =>
      (status: MockingbirdStatusCode.queueEmpty, event: null);

  @override
  MockingbirdPacketResult drainPacket(MockingbirdEngineHandle handle) =>
      (status: MockingbirdStatusCode.queueEmpty, packet: null);

  @override
  MockingbirdStatsResult stats(MockingbirdEngineHandle handle) =>
      (status: MockingbirdStatusCode.ok, stats: zeroStats);

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}
