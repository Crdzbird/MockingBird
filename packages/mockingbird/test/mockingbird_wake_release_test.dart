import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'support/fake_engine_platform.dart';

void main() {
  late FakeEnginePlatform platform;

  setUp(() => MockingbirdPlatform.instance = platform = FakeEnginePlatform());

  MockingbirdEngine engine() =>
      MockingbirdEngine.create(const MockingbirdConfig(backend: .hostPcm));

  test('dispose releases the wake after stop and before destroy', () async {
    final e = engine();
    await e.startAsync();
    await e.dispose();
    expect(platform.calls, [
      'create',
      'start',
      'wake on',
      'stop',
      'wake off',
      'destroy',
    ]);
  });

  test('stop releases the wake even when the native stop fails', () async {
    final e = engine();
    await e.startAsync();
    platform.stopStatus = MockingbirdStatusCode.invalidState;
    await expectLater(e.stopAsync(), throwsA(isA<MockingbirdException>()));
    expect(platform.wake, isNull);
    await expectLater(e.dispose(), completes);
    expect(platform.calls.last, 'destroy');
  });

  test('dispose during a pending start waits for it, then releases', () async {
    final opening = platform.opening = Completer<void>();
    final e = engine();
    final starting = e.startAsync();
    final disposing = e.dispose();
    opening.complete();
    await starting;
    await disposing;
    expect(platform.calls, [
      'create',
      'start',
      'wake on',
      'stop',
      'wake off',
      'destroy',
    ]);
  });

  test('a restart registers a fresh wake and releases the old one', () async {
    final e = engine();
    await e.startAsync();
    await e.stopAsync();
    await e.startAsync();
    await e.dispose();
    expect(platform.calls.where((c) => c.startsWith('wake')), [
      'wake on',
      'wake off',
      'wake on',
      'wake off',
    ]);
    expect(platform.calls.last, 'destroy');
  });
}
