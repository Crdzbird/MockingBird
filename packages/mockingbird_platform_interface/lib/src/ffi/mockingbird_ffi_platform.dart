import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_devices.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_keywords.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_model_downloader.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_speaker.dart';
import 'package:mockingbird_platform_interface/src/ffi/mockingbird_ffi_platform_base.dart';

/// The `dart:ffi` implementation; a platform package supplies the library.
abstract class MockingbirdFfiPlatform extends MockingbirdFfiPlatformBase {
  late final FfiDevices _devices = FfiDevices(bindings);
  late final FfiKeywords _keywords = FfiKeywords(bindings);
  late final FfiSpeaker _speaker = FfiSpeaker(bindings);

  @override
  bool setWakeListener(
    MockingbirdEngineHandle handle,
    void Function()? onWake,
  ) => wake.set(handle as FfiEngineHandle, onWake);

  @override
  void ackWake(MockingbirdEngineHandle handle) =>
      wake.ack(handle as FfiEngineHandle);

  @override
  Future<MockingbirdDevicesResult> listInputDevices(int backend) async =>
      _devices.list(backend);

  @override
  int setInputDevice(MockingbirdEngineHandle handle, String? id) =>
      _devices.select(handle as FfiEngineHandle, id);

  @override
  int setKeywordFrontend(
    MockingbirdEngineHandle handle,
    String? melspectrogram,
    String? embedding,
  ) => _keywords.frontend(handle as FfiEngineHandle, melspectrogram, embedding);

  @override
  MockingbirdKeywordResult addKeyword(
    MockingbirdEngineHandle handle,
    String path,
    double threshold,
  ) => _keywords.add(handle as FfiEngineHandle, path, threshold);

  @override
  int clearKeywords(MockingbirdEngineHandle handle) =>
      _keywords.clear(handle as FfiEngineHandle);

  @override
  Future<String> downloadModel(
    MockingbirdModelSpec spec, {
    required String directory,
    MockingbirdDownloadProgress? onProgress,
  }) => const FfiModelDownloader().fetch(spec, directory, onProgress);

  @override
  int setSpeakerModel(MockingbirdEngineHandle handle, String? path) =>
      _speaker.model(handle as FfiEngineHandle, path);

  @override
  MockingbirdSpeakerEmbeddingResult speakerEmbedding(
    MockingbirdEngineHandle handle,
    Int16List samples,
  ) => _speaker.embedding(handle as FfiEngineHandle, samples);

  @override
  int setTargetSpeaker(
    MockingbirdEngineHandle handle,
    Float32List? embedding,
    double threshold,
  ) => _speaker.target(handle as FfiEngineHandle, embedding, threshold);
}
