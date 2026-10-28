import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_web_base.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';
import 'package:mockingbird_web/src/web_keywords.dart';
import 'package:mockingbird_web/src/web_model_download.dart';
import 'package:mockingbird_web/src/web_model_store.dart';
import 'package:mockingbird_web/src/web_speaker.dart';

/// The model-backed operations of the web platform: keywords, speaker
/// verification and models kept in the module's memory.
abstract class MockingbirdWebModels extends MockingbirdWebBase {
  @override
  int setKeywordFrontend(
    MockingbirdEngineHandle handle,
    String? melspectrogram,
    String? embedding,
  ) => WebKeywords(
    ModuleRegistry.module,
  ).frontend(handle as WebEngineHandle, melspectrogram, embedding);

  @override
  MockingbirdKeywordResult addKeyword(
    MockingbirdEngineHandle handle,
    String path,
    double threshold,
  ) => WebKeywords(
    ModuleRegistry.module,
  ).add(handle as WebEngineHandle, path, threshold);

  @override
  int clearKeywords(MockingbirdEngineHandle handle) =>
      WebKeywords(ModuleRegistry.module).clear(handle as WebEngineHandle);

  @override
  int setSpeakerModel(MockingbirdEngineHandle handle, String? path) =>
      WebSpeaker(ModuleRegistry.module).model(handle as WebEngineHandle, path);

  @override
  MockingbirdSpeakerEmbeddingResult speakerEmbedding(
    MockingbirdEngineHandle handle,
    Int16List samples,
  ) => WebSpeaker(
    ModuleRegistry.module,
  ).embedding(handle as WebEngineHandle, samples);

  @override
  int setTargetSpeaker(
    MockingbirdEngineHandle handle,
    Float32List? embedding,
    double threshold,
  ) => WebSpeaker(
    ModuleRegistry.module,
  ).target(handle as WebEngineHandle, embedding, threshold);

  @override
  Future<String> downloadModel(
    MockingbirdModelSpec spec, {
    required String directory,
    MockingbirdDownloadProgress? onProgress,
  }) async {
    final bytes = await const WebModelDownload().fetch(spec, onProgress);
    return await storeModel(spec.fileName, bytes);
  }

  @override
  Future<String> storeModel(String name, Uint8List bytes) async {
    await initialize();
    return WebModelStore(ModuleRegistry.module).store(name, bytes);
  }
}
