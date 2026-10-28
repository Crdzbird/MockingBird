# Downloadable models

Models the Flutter package fetches on first use instead of bundling, and the
`.onnx` sources of the `.ort` models it bundles (`melspectrogram.onnx`,
`embedding_model.onnx`; Silero VAD's is `tests/fixtures/silero_vad.onnx`). Apps
download them from this directory, at the commit pinned in
`packages/mockingbird_platform_interface/lib/src/mockingbird_model_catalog.dart`,
through raw.githubusercontent.com or jsDelivr, and check each file's SHA-256
before using it. Any app can also bundle a file itself and pass its path.

Each model comes as `.onnx`, which the ONNX Runtime builds with the full
format support load, and as `.ort`, the runtime-optimized format the reduced
ONNX Runtime builds load.

| File | Bytes | SHA-256 |
|---|---|---|
| `yamnet_3s.onnx` | 5041118 | `753401dccd294d6e42b73d24565fb23a54198772a025d4266f5123793b6b37ff` |
| `yamnet_3s.ort` | 5080664 | `2f4970ebb8891e5562fdfe1b6ef0d305f57319ed8106873526d95ca0e154b296` |
| `speaker_campplus.onnx` | 8350452 | `ed328c590c407e4bbad12a915cef896d832c17e479fe1f393e74487d29105968` |
| `speaker_campplus.ort` | 8650808 | `1618f1f3787e59cece8b237f48f6ffb636d7b06fca0a527043e289d06e5baea5` |

## Sources

- `yamnet_3s`: Google's YAMNet sound classifier (Apache-2.0) exported with a
  3 s input, its convolution and dense weights stored as int8 by
  `scripts/quantize_yamnet.py`.
- `speaker_campplus`: CAM++ speaker embedding model from Alibaba's 3D-Speaker,
  trained on about 200,000 Chinese and English speakers
  (`iic/speech_campplus_sv_zh_en_16k-common_advanced` on ModelScope,
  Apache-2.0), in the ONNX export sherpa-onnx publishes, its convolution
  weights stored as int8 by the same script. On 40 LibriSpeech test-clean
  speakers, 200 target and 7,800 impostor trials, the float and int8 models
  both separate every speaker (0% equal error rate).

The `.ort` files come from `scripts/convert_models_ort.sh`.
