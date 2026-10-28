import 'package:mockingbird_platform_interface/src/mockingbird_model_spec.dart';

/// The models the plugin downloads on first use, pinned to one commit of the
/// repository's `models/` directory and served by GitHub and jsDelivr.
final class MockingbirdModelCatalog {
  const MockingbirdModelCatalog._();

  /// The commit the files are pinned to.
  static const commit = '9f8a3c1755acd6692d775dd65a4b7cd550a4ecde';

  /// YAMNet with int8 weights, for ONNX Runtime builds that read `.onnx`.
  static final MockingbirdModelSpec yamnetOnnx = _spec(
    'yamnet_3s.onnx',
    5041118,
    '753401dccd294d6e42b73d24565fb23a54198772a025d4266f5123793b6b37ff',
  );

  /// YAMNet in ONNX Runtime's `.ort` format.
  static final MockingbirdModelSpec yamnetOrt = _spec(
    'yamnet_3s.ort',
    5080664,
    '2f4970ebb8891e5562fdfe1b6ef0d305f57319ed8106873526d95ca0e154b296',
  );

  /// The CAM++ speaker embedding model, for ONNX Runtime builds that read
  /// `.onnx`.
  static final MockingbirdModelSpec speakerOnnx = _spec(
    'speaker_campplus.onnx',
    8350452,
    'ed328c590c407e4bbad12a915cef896d832c17e479fe1f393e74487d29105968',
  );

  /// The CAM++ speaker embedding model in `.ort` format.
  static final MockingbirdModelSpec speakerOrt = _spec(
    'speaker_campplus.ort',
    8650808,
    '1618f1f3787e59cece8b237f48f6ffb636d7b06fca0a527043e289d06e5baea5',
  );

  static MockingbirdModelSpec _spec(String file, int bytes, String sha256) =>
      MockingbirdModelSpec(
        fileName: file,
        bytes: bytes,
        sha256: sha256,
        urls: [
          Uri.parse(
            'https://raw.githubusercontent.com/Crdzbird/MockingBird/'
            '$commit/models/$file',
          ),
          Uri.parse(
            'https://cdn.jsdelivr.net/gh/Crdzbird/MockingBird@$commit/'
            'models/$file',
          ),
        ],
      );
}
