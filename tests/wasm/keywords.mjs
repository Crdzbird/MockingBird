// Runs keyword spotting inside a WebAssembly build that links ONNX Runtime:
// the front-end models and tests/fixtures/keyword_test.ort go into the
// module's memory file system, then hello.wav goes through the engine. At a
// 0.9 threshold the test keyword fires once, at the 27th 80 ms step, as it
// does natively.
//   node tests/wasm/keywords.mjs build-wasm-simd
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

function store(file) {
  const bytes = readFileSync(file);
  const data = m._malloc(bytes.length);
  m.HEAPU8.set(bytes, data);
  const name = `/models/${path.basename(file)}`;
  const s = m._mockingbird_web_store_file(cString(name), data, bytes.length);
  m._free(data);
  if (s !== 0) throw new Error(`store ${file}: ${s}`);
  return cString(name);
}

function wavSamples(file) {
  const b = readFileSync(file);
  for (let off = 12; off + 8 <= b.length;) {
    const size = b.readUInt32LE(off + 4);
    if (b.toString('ascii', off, off + 4) === 'data') {
      return new Int16Array(b.buffer.slice(b.byteOffset + off + 8, b.byteOffset + off + 8 + size));
    }
    off += 8 + size + (size & 1);
  }
  throw new Error('no data chunk');
}

const assets = path.join(root, 'packages', 'mockingbird', 'assets');
const cfg = m._mockingbird_web_config_new();
for (const [field, value] of [[0, 4], [4, 0], [9, 0], [23, 1 << 20]]) m._mockingbird_web_config_set(cfg, field, value);
const statusPtr = m._malloc(4);
const engine = m._mockingbird_web_create(cfg, statusPtr);
m._mockingbird_web_config_free(cfg);
if (!engine) throw new Error(`create failed: ${m.HEAP32[statusPtr >> 2]}`);
const mel = store(path.join(assets, 'melspectrogram.ort'));
const emb = store(path.join(assets, 'embedding_model.ort'));
if (m._mockingbird_set_keyword_frontend(engine, mel, emb) !== 0) throw new Error('frontend failed');
const idPtr = m._malloc(4);
const keyword = store(path.join(root, 'tests', 'fixtures', 'keyword_test.ort'));
if (m._mockingbird_add_keyword(engine, keyword, 0.9, idPtr) !== 0) throw new Error('add failed');
if (m._mockingbird_start(engine) !== 0) throw new Error('start failed');

const buf = m._malloc(640);
const evt = m._mockingbird_web_event_new();
const pkt = m._mockingbird_web_packet_new();
const hits = [];
const drain = () => {
  while (m._mockingbird_poll_event(engine, evt) === 0) {
    const type = m._mockingbird_web_event_get(evt, 0);
    if (type === 13) hits.push(m._mockingbird_web_event_get(evt, 2));
    if (type === 7) throw new Error(`backend error ${m._mockingbird_web_event_get(evt, 3)}`);
  }
  while (m._mockingbird_drain(engine, pkt) === 0) m._mockingbird_release(engine, pkt);
};
const pcm = wavSamples(path.join(root, 'tests', 'fixtures', 'hello.wav'));
const t0 = performance.now();
for (let i = 0; i + 320 <= pcm.length; i += 320) {
  m.HEAP16.set(pcm.subarray(i, i + 320), buf >> 1);
  m._mockingbird_web_push_pcm(engine, buf, 320, (i / 16) * 1000);
  drain();
}
const share = (performance.now() - t0) / 1000 / (pcm.length / 16000);
m._mockingbird_stop(engine);
drain();
m._mockingbird_destroy(engine);
console.log(JSON.stringify({ keyword_hits_us: hits, cpu_share: share }));
if (hits.length !== 1 || hits[0] >= 27 * 80000 || hits[0] + 20000 < 27 * 80000) {
  console.error('expected one keyword event on the 27th step');
  process.exit(1);
}
console.log('wasm keywords: PASS');
