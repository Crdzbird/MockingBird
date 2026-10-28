# Test fixtures

| File | Source |
|---|---|
| `speaker_a1.wav`, `speaker_a2.wav` | The first 5 s of utterances 1089-134686-0002 and 1089-134686-0005 (speaker 1089, Peter Bobbe) from LibriSpeech test-clean |
| `speaker_b.wav` | The first 5 s of utterance 121-121726-0001 (speaker 121, Nikolle Doolin) from LibriSpeech test-clean |
| `keyword_test.onnx`, `keyword_test.ort` | Built by `scripts/make_keyword_test_model.py` |
| `silero_vad.onnx`, `yamnet_3s.onnx` | Copies of the models described in the top-level README |

LibriSpeech is by Vassil Panayotov, Guoguo Chen, Daniel Povey and Sanjeev
Khudanpur, from LibriVox recordings, and is licensed under CC BY 4.0
(https://www.openslr.org/12). The clips were converted to 16 kHz mono 16-bit
WAV and cut to 5 s.
