import 'dart:typed_data';

import 'package:auris_platform_interface/src/auris_engine_handle.dart';
import 'package:auris_platform_interface/src/auris_native_config.dart';
import 'package:auris_platform_interface/src/auris_native_event.dart';
import 'package:auris_platform_interface/src/auris_native_packet.dart';
import 'package:auris_platform_interface/src/auris_native_stats.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

/// The result of creating an engine: a status code and, on success, a handle.
typedef AurisCreateResult = ({int status, AurisEngineHandle? handle});

/// A status code with the samples a PCM tap read.
typedef AurisPcmResult = ({int status, Int16List samples});

/// A status code with the event polled, if any.
typedef AurisEventResult = ({int status, AurisNativeEvent? event});

/// A status code with the packet drained, if any.
typedef AurisPacketResult = ({int status, AurisNativePacket? packet});

/// A status code with the stats snapshot, if read.
typedef AurisStatsResult = ({int status, AurisNativeStats? stats});

/// The operations a platform implementation provides over the engine.
///
/// Every method returns the C ABI status code as an integer; the `auris`
/// package turns non-zero codes into exceptions.
abstract class AurisPlatform extends PlatformInterface {
  /// Constructs a platform with the verification token.
  AurisPlatform() : super(token: _token);

  static final Object _token = Object();
  static AurisPlatform? _instance;

  /// The registered implementation.
  static AurisPlatform get instance {
    final i = _instance;
    if (i == null) {
      throw StateError(
        'No AurisPlatform implementation has been registered. The platform '
        'plugin must call `AurisPlatform.instance = ...` during startup.',
      );
    }
    return i;
  }

  /// Registers the implementation for the running platform.
  static set instance(AurisPlatform value) {
    PlatformInterface.verify(value, _token);
    _instance = value;
  }

  /// Prepares the platform; the web implementation loads its module here.
  Future<void> initialize() async {}

  /// The linked engine version, `X.Y.Z`.
  String version();

  /// The engine's own name for a status code.
  String statusName(int status);

  /// Creates an engine for [config].
  AurisCreateResult create(AurisNativeConfig config);

  /// Destroys an engine; stops it first if needed.
  void destroy(AurisEngineHandle handle);

  /// Starts capture or accepting pushed PCM.
  int start(AurisEngineHandle handle);

  /// Stops the engine; every frame pushed before is processed.
  int stop(AurisEngineHandle handle);

  /// Pushes mono int16 samples for a host-PCM engine.
  int pushPcm(AurisEngineHandle handle, Int16List samples, int captureTsUs);

  /// Reads up to [maxSamples] from the ungated PCM tap.
  AurisPcmResult readPcm(AurisEngineHandle handle, int maxSamples);

  /// Takes the next event, or `queueEmpty`.
  AurisEventResult pollEvent(AurisEngineHandle handle);

  /// Takes the next packet, copied, or `queueEmpty`.
  AurisPacketResult drainPacket(AurisEngineHandle handle);

  /// Reads a stats snapshot.
  AurisStatsResult stats(AurisEngineHandle handle);

  /// Retunes a running engine; structure must match.
  int updateConfig(AurisEngineHandle handle, AurisNativeConfig config);
}
