# Accuracy benchmark

Measures how well Mockingbird's detectors do their job, next to the pub.dev
packages that do the same job, on public datasets. `bench/flutter_compare`
measures what each package costs; this directory measures whether it is right.

| Script | What it measures |
|---|---|
| `vad_eval.py` | voice activity detection: speech frames found, noise taken for speech, at five signal-to-noise ratios |
| `classify_eval.py` | sound classification on ESC-50: YAMNet as Mockingbird ships it, the float model it came from, and CED tiny as sherpa_onnx runs it |
| `speaker_eval.py` | speaker verification on 40 LibriSpeech speakers, by test length and noise |

## Data

Neither dataset is committed. Both are downloaded once into a cache directory
outside the repository:

| Dataset | Size | Licence | Used for |
|---|---|---|---|
| [LibriSpeech test-clean](https://www.openslr.org/12) | 346 MB | CC BY 4.0 | speech |
| [ESC-50](https://github.com/karolpiczak/ESC-50) | 646 MB | CC BY-NC 3.0 | noise for the VAD mixtures, and the classification clips |

```bash
mkdir -p ~/.cache/mockingbird-bench && cd ~/.cache/mockingbird-bench
curl -LO https://www.openslr.org/resources/12/test-clean.tar.gz
curl -L -o esc50.zip https://github.com/karolpiczak/ESC-50/archive/master.zip
```

`prepare.py` unpacks both and writes 16 kHz mono WAV copies with ffmpeg: 60
LibriSpeech utterances, taken round-robin across the 40 speakers, six
utterances longer than 4 s from each speaker, and all 2000 ESC-50 clips.

## Running

Needs Python 3.12 with `numpy`, `onnxruntime` and `sherpa-onnx` (measured with
2.5.4, 1.31.1 and 2.0.1), ffmpeg on `PATH`, and the release build of the
engine in `build/` for the Python binding.

```bash
python3 bench/accuracy/prepare.py ~/.cache/mockingbird-bench
python3 bench/accuracy/vad_eval.py ~/.cache/mockingbird-bench
git show 2966238~1:packages/mockingbird/assets/yamnet_3s.onnx > /tmp/yamnet_float.onnx
python3 bench/accuracy/classify_eval.py ~/.cache/mockingbird-bench /tmp/yamnet_float.onnx
python3 bench/accuracy/speaker_eval.py ~/.cache/mockingbird-bench
```

The classification run also needs the CED tiny model that
`bench/flutter_compare/README.md` tells you how to fetch. Results land in
`results/vad.json`, `results/classify.json` and `results/speaker.json`; the committed copies are the
ones quoted below.

## Voice activity detection

### Method

- The 60 utterances are joined with 1 to 3 s of silence between them, 11.2
  minutes in all, 71% of it speech.
- Ground truth comes from the clean signal on the 20 ms frame grid: a frame is
  speech when its level is within 35 dB of the loudest frame of its utterance
  and above -55 dBFS. Pauses shorter than 200 ms are filled and runs shorter
  than 60 ms dropped, so the labels follow words, not syllables.
- Noise is ESC-50 clips that contain no human voice (the crying, laughing,
  coughing, sneezing, breathing, snoring and sipping categories are left out),
  scaled to 20, 10, 5 and 0 dB below the speech.
- Every system turns the same mixture into speech intervals, scored frame by
  frame: precision (speech it reported that was speech), recall (speech it
  found), F1, and the false-alarm rate (non-speech frames it called speech).
- Mockingbird runs as the engine itself through the Python binding, with
  default settings except that the anomaly gate is off. Speech runs from each
  SPEECH_BEGIN to the matching SPEECH_END minus the hangover, because
  SPEECH_END fires when the hangover confirms the end and the other packages
  report where it began.
- The other packages run their own Silero model files through ONNX Runtime
  with the decision logic each one publishes, at its default settings:
  `state_machines.py` has both. vad and sherpa_onnx ship Silero v5, vad_plus
  ships the same Silero v6.2 file Mockingbird does.

The false-alarm rate is high for every system even on clean audio because
every one of them keeps speech open for a while after the last word, which the
labels count as non-speech. Compare it across systems, not against zero.

### Results

| System | Clean F1 | 10 dB F1 | 0 dB F1 | 0 dB precision | 0 dB recall | 0 dB false alarm |
|---|---|---|---|---|---|---|
| Mockingbird fast (libfvad) | 0.903 | 0.862 | 0.855 | 0.760 | 0.977 | 0.773 |
| Mockingbird neural (Silero v6.2) | 0.876 | 0.867 | 0.842 | 0.860 | 0.824 | 0.337 |
| Mockingbird neural, 768 ms hangover | 0.899 | 0.890 | 0.864 | 0.856 | 0.872 | 0.369 |
| vad (Silero v5) | 0.902 | 0.899 | 0.863 | 0.882 | 0.846 | 0.284 |
| vad_plus (Silero v6) | 0.907 | 0.897 | 0.873 | 0.851 | 0.895 | 0.391 |
| sherpa_onnx (Silero v5) | 0.887 | 0.882 | 0.838 | 0.879 | 0.801 | 0.277 |

What the numbers say:

- The fast tier is as good as any of them in quiet rooms and the wrong choice
  in noise: at 0 dB it calls 77% of the noise speech. That is the case the
  neural tier exists for.
- The neural tier's default hangover is 300 ms, so SPEECH_END arrives 470 ms
  sooner than with vad_plus's 768 ms redemption window. That costs recall on
  the pauses inside sentences. With the hangover matched, it is within one F1
  point of vad_plus on the same model at every noise level; the rest is
  vad_plus starting each utterance 96 ms before the first window over the
  threshold, which Mockingbird does in its pre-roll instead of in the event.
- Silero v5 holds up slightly better than v6.2 at 0 dB on this data, in both
  the vad package and sherpa_onnx's logic around it.

The first run of this benchmark showed the neural tier stamping SPEECH_BEGIN
on the frame that confirmed speech, a window or two late. It now stamps the
first window over the threshold, which raised its recall by 1.4 to 1.5 points at
every noise level with no loss of precision.

## Sound classification

### Method

- 1880 ESC-50 clips, 5 s each, in 47 of the 50 categories. Drinking and
  sipping, can opening and washing machines have no AudioSet class in YAMNet or
  CED, so they are left out.
- Both taggers predict AudioSet's 521 or 527 classes, not ESC-50's, so the
  clips are scored zero-shot: `esc50_audioset.py` lists, for each ESC-50
  category, the AudioSet classes that name the same sound, with no class shared
  between two categories. A category scores the highest score among its
  classes, and a clip is correct when its own category ranks first, or in the
  first three, of the 47.
- YAMNet runs the way Mockingbird runs it, on 3 s windows (here at 0, 1 and 2
  s), taking each class's highest score across the windows. CED tiny runs
  through sherpa_onnx's audio tagging API on the whole clip.

### Results

| Model | File | Top-1 | Top-3 | Single-thread cost per 3 s |
|---|---|---|---|---|
| Mockingbird YAMNet, int8 weights | 5.0 MB | 69.5% | 88.4% | 24 ms |
| YAMNet, float weights | 16.1 MB | 69.3% | 88.5% | 24 ms |
| sherpa_onnx CED tiny | 22.3 MB (6.1 MB int8) | 82.9% | 95.9% | 19 ms |

- Storing YAMNet's weights as int8 cost no accuracy: the two differ by fewer
  clips than one category holds.
- CED tiny is 13 points better at top-1 for about the same compute. YAMNet's
  weakest categories are mouse clicks, wooden door creaks, roosters and wind;
  CED's are hens, crickets and wind.

## Speaker verification

### Method

- Each of the 40 speakers is enrolled with the engine's voice print of one
  clean utterance longer than 4 s.
- Their other five utterances are cut to 1, 2 or 4 s, starting 0.3 s in,
  mixed with ESC-50 sounds that contain no voice at the given SNR, and scored
  against all 40 enrollments: 200 target and 7,800 impostor trials per row.
- The equal error rate is where false rejects and false accepts meet; the
  other columns use fixed thresholds on the cosine similarity.
- Then 40 whole utterances at 10 dB run through the engine itself, with the
  neural voice detector cutting them into utterances and the verifier
  reporting each one, once against their own speaker and once against another.

### Results

| Test audio | EER | Threshold 0.5: false reject | false accept | Threshold 0.4: false reject | false accept |
|---|---|---|---|---|---|
| clean, 1 s | 0.50% | 8.0% | 0% | 0.5% | 0.35% |
| clean, 2 s | 0% | 0% | 0% | 0% | 0.44% |
| clean, 4 s | 0% | 0% | 0.01% | 0% | 0.71% |
| 10 dB, 1 s | 4.5% | 26.0% | 0.01% | 10.0% | 0.32% |
| 10 dB, 2 s | 3.0% | 6.5% | 0% | 3.0% | 0.37% |
| 10 dB, 4 s | 0.01% | 0% | 0.01% | 0% | 0.62% |
| 0 dB, 1 s | 7.0% | 50.5% | 0% | 24.0% | 0.15% |
| 0 dB, 2 s | 2.0% | 11.0% | 0.01% | 4.5% | 0.27% |
| 0 dB, 4 s | 0.74% | 2.5% | 0.01% | 1.0% | 0.64% |

Through the engine, voice detection cut the 40 utterances into 82 pieces. No
impostor piece reached 0.5 (the highest scored 0.43); 5 of the 82 genuine
pieces fell below it, all short ones.

What the numbers say: two seconds of speech is enough in most rooms, one
second is not in noise, and 0.5 keeps strangers out at the cost of asking the
enrolled person to repeat a short phrase now and then. LibriSpeech is read
English with a clean microphone, so expect more misses with far-field audio or
in other languages, although the model was trained on Chinese and English.

