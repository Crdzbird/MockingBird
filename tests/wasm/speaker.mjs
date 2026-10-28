// Speaker verification inside a WebAssembly build that links ONNX Runtime:
// the CAM++ model goes into the module's memory file system, voice prints
// are computed for the two LibriSpeech speakers in tests/fixtures, and the
// same speaker scores high while the other scores low, as natively.
//   node tests/wasm/speaker.mjs build-wasm-simd
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');
const dir = path.resolve(root, process.argv[2] ?? 'build-wasm-simd');
const { default: create } = await import(path.join(dir, 'mockingbird.js'));
const m = await create();

function cString(s) {
  const bytes = new TextEncoder().encode(s + '\0');
  const p = m._malloc(bytes.length);
  m.HEAPU8.set(bytes, p);
  return p;
}

const model = readFileSync(path.join(root, 'models', 'speaker_campplus.ort'));
const data = m._malloc(model.length);
m.HEAPU8.set(model, data);
if (m._mockingbird_web_store_file(cString('/models/speaker_campplus.ort'), data, model.length) !== 0) {
  throw new Error('store failed');
}
m._free(data);

const cfg = m._mockingbird_web_config_new();
m._mockingbird_web_config_set(cfg, 0, 4);
const status = m._malloc(4);
const engine = m._mockingbird_web_create(cfg, status);
m._mockingbird_web_config_free(cfg);
if (!engine) throw new Error(`create failed: ${m.HEAP32[status >> 2]}`);
if (m._mockingbird_set_speaker_model(engine, cString('/models/speaker_campplus.ort')) !== 0) throw new Error('model');

function voicePrint(name) {
  const b = readFileSync(path.join(root, 'tests', 'fixtures', name));
  const pcm = new Int16Array(b.buffer.slice(b.byteOffset + 44, b.byteOffset + b.length));
  const buf = m._malloc(pcm.length * 2);
  m.HEAP16.set(pcm, buf >> 1);
  const out = m._malloc(256 * 4);
  const dim = m._malloc(4);
  const s = m._mockingbird_speaker_embedding(engine, buf, pcm.length, out, 256, dim);
  if (s !== 0) throw new Error(`embedding ${name}: ${s}`);
  const v = Float32Array.from(m.HEAPF32.subarray(out >> 2, (out >> 2) + m.HEAP32[dim >> 2]));
  m._free(buf);
  m._free(out);
  m._free(dim);
  return v;
}

const dot = (a, b) => a.reduce((s, x, i) => s + x * b[i], 0);
const a1 = voicePrint('speaker_a1.wav');
const a2 = voicePrint('speaker_a2.wav');
const b = voicePrint('speaker_b.wav');
m._mockingbird_destroy(engine);
console.log(JSON.stringify({ dim: a1.length, same: dot(a1, a2), other: dot(a1, b) }));
if (a1.length !== 192 || dot(a1, a2) < 0.6 || dot(a1, b) > 0.3) {
  console.error('expected 192 values, the same speaker above 0.6 and the other below 0.3');
  process.exit(1);
}
console.log('wasm speaker: PASS');
