import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/mockingbird_download_progress.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_model_spec.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_device.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

/// The operations a platform may leave out; each default says it cannot.
abstract class MockingbirdPlatformExtras extends PlatformInterface {
  /// Passes the verification [token] up.
  MockingbirdPlatformExtras({required super.token});

  /// Calls [onWake] on this isolate soon after the engine queues an event or
  /// packet, or stops when [onWake] is null. False when this platform cannot
  /// push, in which case the caller polls on a timer.
  bool setWakeListener(
    MockingbirdEngineHandle handle,
    void Function()? onWake,
  ) => false;

  /// Re-arms the wake listener; call it before draining.
  void ackWake(MockingbirdEngineHandle handle) {}

  /// The capture devices of `mockingbird_backend_t` [backend].
  Future<MockingbirdDevicesResult> listInputDevices(int backend) async => (
    status: MockingbirdStatusCode.unsupported,
    devices: const <MockingbirdNativeDevice>[],
  );

  /// Chooses the device the next start opens; null follows the default.
  int setInputDevice(MockingbirdEngineHandle handle, String? id) =>
      MockingbirdStatusCode.unsupported;

  /// Names the two models every keyword shares; null uses the defaults.
  int setKeywordFrontend(
    MockingbirdEngineHandle handle,
    String? melspectrogram,
    String? embedding,
  ) => MockingbirdStatusCode.unsupported;

  /// Registers the keyword model at [path] and returns its id.
  MockingbirdKeywordResult addKeyword(
    MockingbirdEngineHandle handle,
    String path,
    double threshold,
  ) => (status: MockingbirdStatusCode.unsupported, id: -1);

  /// Removes every keyword.
  int clearKeywords(MockingbirdEngineHandle handle) =>
      MockingbirdStatusCode.unsupported;

  /// Names the speaker embedding model; null uses the default.
  int setSpeakerModel(MockingbirdEngineHandle handle, String? path) =>
      MockingbirdStatusCode.unsupported;

  /// The voice print of 16 kHz mono [samples].
  MockingbirdSpeakerEmbeddingResult speakerEmbedding(
    MockingbirdEngineHandle handle,
    Int16List samples,
  ) => (status: MockingbirdStatusCode.unsupported, embedding: Float32List(0));

  /// Sets the voice print utterances are compared with; null stops.
  int setTargetSpeaker(
    MockingbirdEngineHandle handle,
    Float32List? embedding,
    double threshold,
  ) => MockingbirdStatusCode.unsupported;

  /// Downloads [spec], checks it, keeps it under [directory], and returns
  /// the path the engine opens it from. A kept copy is reused without any
  /// network access.
  Future<String> downloadModel(
    MockingbirdModelSpec spec, {
    required String directory,
    MockingbirdDownloadProgress? onProgress,
  }) => throw UnsupportedError('downloadModel is not available here.');

  /// Stores model [bytes] where the engine can open them and returns the
  /// path; only platforms without a file system need it.
  Future<String> storeModel(String name, Uint8List bytes) =>
      throw UnsupportedError('storeModel is only needed on the web.');
}
