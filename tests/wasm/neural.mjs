// Runs Silero VAD and YAMNet inside a WebAssembly build that links ONNX
// Runtime: the .ort models go into the module's memory file system, then the
// speech, cat and music fixtures go through the engine. The detections match
// the native build's for the same audio.
//   node tests/wasm/neural.mjs build-wasm-simd
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

function store(name) {
  const dir = name.startsWith('yamnet') ? ['models'] : ['packages', 'mockingbird', 'assets'];
  const bytes = readFileSync(path.join(root, ...dir, name));
  const data = m._malloc(bytes.length);
  m.HEAPU8.set(bytes, data);
  const p = cString(`/models/${name}`);
  const s = m._mockingbird_web_store_file(p, data, bytes.length);
  m._free(data);
  if (s !== 0) throw new Error(`store ${name}: ${s}`);
  return p;
}

function wavSamples(file) {
  const b = readFileSync(file);
  let off = 12;
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4);
    const size = b.readUInt32LE(off + 4);
    if (id === 'data') return new Int16Array(b.buffer.slice(b.byteOffset + off + 8, b.byteOffset + off + 8 + size));
    off += 8 + size + (size & 1);
  }
  throw new Error('no data chunk');
}

const F = { backend: 0, enableVad: 4, vadTier: 5, enableAnomaly: 9, enableSoundClass: 13, pcmRingFrames: 23, packetQueueCapacity: 24 };
const cfg = m._mockingbird_web_config_new();
for (const [field, value] of [[F.backend, 4], [F.enableVad, 1], [F.vadTier, 1], [F.enableAnomaly, 0], [F.enableSoundClass, 1], [F.pcmRingFrames, 1 << 20], [F.packetQueueCapacity, 8192]]) {
  m._mockingbird_web_config_set(cfg, field, value);
}
m._mockingbird_web_config_set_path(cfg, 0, store('silero_vad.ort'));
m._mockingbird_web_config_set_path(cfg, 1, store('yamnet_3s.ort'));
const statusPtr = m._malloc(4);
const engine = m._mockingbird_web_create(cfg, statusPtr);
m._mockingbird_web_config_free(cfg);
if (!engine) throw new Error(`create failed: ${m.HEAP32[statusPtr >> 2]}`);
if (m._mockingbird_start(engine) !== 0) throw new Error('start failed');

const buf = m._malloc(640);
const evt = m._mockingbird_web_event_new();
const pkt = m._mockingbird_web_packet_new();
let speech = 0;
const classes = [];
const drain = () => {
  while (m._mockingbird_poll_event(engine, evt) === 0) {
    const type = m._mockingbird_web_event_get(evt, 0);
    if (type === 1) speech++;
    if (type === 8) classes.push(m._mockingbird_web_event_get(evt, 3));
    if (type === 7) throw new Error(`backend error ${m._mockingbird_web_event_get(evt, 3)}`);
  }
  while (m._mockingbird_drain(engine, pkt) === 0) m._mockingbird_release(engine, pkt);
};
let ts = 0;
const t0 = performance.now();
let seconds = 0;
for (const name of ['hello.wav', 'miaow_16k.wav', 'music.wav']) {
  const pcm = wavSamples(path.join(root, 'tests', 'fixtures', name));
  seconds += pcm.length / 16000;
  for (let i = 0; i + 320 <= pcm.length; i += 320, ts += 20000) {
    m.HEAP16.set(pcm.subarray(i, i + 320), buf >> 1);
    m._mockingbird_web_push_pcm(engine, buf, 320, ts);
    drain();
  }
}
const ms = performance.now() - t0;
m._mockingbird_stop(engine);
drain();
m._mockingbird_destroy(engine);
console.log(JSON.stringify({ speech_begin: speech, sound_classes: classes.length, classes: classes.join(' '), cpu_share: ms / 1000 / seconds }));
const expected = '494 0 0 0 0 0 67 67 67 67 67 67 132 132 132';
if (speech !== 3 || classes.join(' ') !== expected) {
  console.error(`expected 3 speech starts and classes ${expected}`);
  process.exit(1);
}
console.log('wasm neural: PASS');
