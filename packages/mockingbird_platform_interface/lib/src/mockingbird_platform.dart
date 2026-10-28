import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/mockingbird_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_config.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_platform_extras.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

/// The operations a platform implementation provides over the engine. Each
/// returns the C ABI status code; `mockingbird` turns non-zero into errors.
abstract class MockingbirdPlatform extends MockingbirdPlatformExtras {
  /// Constructs a platform with the verification token.
  MockingbirdPlatform() : super(token: _token);

  static final Object _token = Object();
  static MockingbirdPlatform? _instance;

  /// The registered implementation.
  static MockingbirdPlatform get instance {
    final i = _instance;
    if (i == null) {
      throw StateError(
        'No MockingbirdPlatform implementation has been registered. '
        'The platform '
        'plugin must call `MockingbirdPlatform.instance = ...` during startup.',
      );
    }
    return i;
  }

  /// Registers the implementation for the running platform.
  static set instance(MockingbirdPlatform value) {
    PlatformInterface.verify(value, _token);
    _instance = value;
  }

  /// Prepares the platform; the web implementation loads its module.
  Future<void> initialize() async {}

  /// The linked engine version, `X.Y.Z`.
  String version();

  /// The engine's own name for a status code.
  String statusName(int status);

  /// Creates an engine for [config].
  MockingbirdCreateResult create(MockingbirdNativeConfig config);

  /// Destroys an engine; stops it first if needed.
  void destroy(MockingbirdEngineHandle handle);

  /// Starts capture or accepting pushed PCM.
  int start(MockingbirdEngineHandle handle);

  /// Stops the engine after processing every pushed frame.
  int stop(MockingbirdEngineHandle handle);

  /// Pushes mono int16 samples for a host-PCM engine.
  int pushPcm(
    MockingbirdEngineHandle handle,
    Int16List samples,
    int captureTsUs,
  );

  /// Reads up to [maxSamples] from the ungated PCM tap.
  MockingbirdPcmResult readPcm(MockingbirdEngineHandle handle, int maxSamples);

  /// Takes the next event, or `queueEmpty`.
  MockingbirdEventResult pollEvent(MockingbirdEngineHandle handle);

  /// Takes the next packet, copied, or `queueEmpty`.
  MockingbirdPacketResult drainPacket(MockingbirdEngineHandle handle);

  /// Reads a stats snapshot.
  MockingbirdStatsResult stats(MockingbirdEngineHandle handle);

  /// Retunes a running engine; the structure must match.
  int updateConfig(
    MockingbirdEngineHandle handle,
    MockingbirdNativeConfig config,
  );
}
