## Unreleased

- `downloadModel` fetches models with retries and a SHA-256 check and keeps them in Cache Storage for later visits.
- Keyword spotting in the browser: the reduced ONNX Runtime now carries the operators of the keyword front end and of openWakeWord keyword models, which load as `.ort` files.
- Silero VAD and YAMNet run in the browser: both builds link a minimal ONNX Runtime, and `storeModel` puts the `.ort` models in the module's memory. Each build now has its own glue file.
- Voice processing becomes `getUserMedia` constraints, and the applied settings are read back into the stats. `listInputDevices` uses `enumerateDevices`, and a selected device is opened exactly.
- A second build of the engine with WebAssembly SIMD, picked automatically where the browser supports it: noise suppression costs 1.89% of a core instead of 3.59%, and the whole web pipeline 2.24% instead of 3.90%.
- The module factory now receives its options as an argument. Before, `locateFile` was silently ignored and the engine only loaded because it sat next to the script.
- WebAssembly engine with the Ogg Opus encoder and segment-end fixes.

## 0.1.0

First release.
