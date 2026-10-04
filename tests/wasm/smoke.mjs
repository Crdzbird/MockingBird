// Pushes the speech fixture through the WebAssembly build with fast VAD,
// RNNoise and Opus on, and checks events, packets and stats come back.
//   node tests/wasm/smoke.mjs
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');
const { default: createAurisModule } = await import(path.join(root, 'build-wasm', 'auris.js'));
const m = await createAurisModule();

function wavSamples(file) {
  const b = readFileSync(file);
  let off = 12;
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4);
    const size = b.readUInt32LE(off + 4);
    if (id === 'data') return new Int16Array(b.buffer, b.byteOffset + off + 8, size / 2);
    off += 8 + size + (size & 1);
  }
  throw new Error('no data chunk');
}

const F = { backend: 0, enableRnnoise: 3, enableVad: 4, codec: 19, pcmRingFrames: 23 };
const cfg = m._auris_web_config_new();
m._auris_web_config_set(cfg, F.backend, 4);
m._auris_web_config_set(cfg, F.enableVad, 1);
m._auris_web_config_set(cfg, F.enableRnnoise, 1);
m._auris_web_config_set(cfg, F.codec, 1);
m._auris_web_config_set(cfg, F.pcmRingFrames, 1 << 16);
const statusPtr = m._malloc(4);
const engine = m._auris_web_create(cfg, statusPtr);
const createStatus = m.HEAP32[statusPtr >> 2];
m._auris_web_config_free(cfg);
if (!engine) throw new Error(`auris_create failed: ${createStatus}`);
if (m._auris_start(engine) !== 0) throw new Error('auris_start failed');

const pcm = wavSamples(path.join(root, 'tests', 'fixtures', 'hello.wav'));
const chunk = 320;
const buf = m._malloc(chunk * 2);
const evt = m._auris_web_event_new();
const pkt = m._auris_web_packet_new();
const counts = {};
let packets = 0, bytes = 0;
const t0 = performance.now();
for (let i = 0, ts = 0; i + chunk <= pcm.length; i += chunk, ts += 20000) {
  m.HEAP16.set(pcm.subarray(i, i + chunk), buf >> 1);
  if (m._auris_web_push_pcm(engine, buf, chunk, ts) !== 0) throw new Error('push failed');
  while (m._auris_poll_event(engine, evt) === 0) {
    const type = m._auris_web_event_get(evt, 0);
    counts[type] = (counts[type] ?? 0) + 1;
  }
  while (m._auris_drain(engine, pkt) === 0) {
    packets++;
    bytes += m._auris_web_packet_get(pkt, 5);
    m._auris_release(engine, pkt);
  }
}
const ms = performance.now() - t0;
m._auris_stop(engine);
while (m._auris_poll_event(engine, evt) === 0) {
  const type = m._auris_web_event_get(evt, 0);
  counts[type] = (counts[type] ?? 0) + 1;
}
const stats = m._auris_web_stats_new();
m._auris_get_stats(engine, stats);
const captured = m._auris_web_stats_get(stats, 0);
const dropped = m._auris_web_stats_get(stats, 1);
const rms = m._auris_web_stats_get(stats, 8);
m._auris_web_stats_free(stats);
m._auris_web_event_free(evt);
m._auris_web_packet_free(pkt);
m._free(buf);
m._free(statusPtr);
m._auris_destroy(engine);

const seconds = pcm.length / 16000;
console.log(JSON.stringify({ version: m.UTF8ToString(m._auris_version_string()), seconds, captured, dropped, rms, packets, bytes, events: counts, cpu_share: ms / 1000 / seconds }));
if (!(counts[1] >= 1 && counts[4] >= 1 && packets > 0 && dropped === 0)) {
  console.error('expected speech and segment events with packets and no drops');
  process.exit(1);
}
console.log('wasm smoke: PASS');
