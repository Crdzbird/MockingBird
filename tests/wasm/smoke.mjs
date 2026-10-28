// Pushes the speech fixture through the WebAssembly build with fast VAD,
// RNNoise and Opus on, and checks events, packets and stats come back, and
// that closed segments end on a LAST packet. Pass `ogg` to run Ogg Opus and
// check that a segment's first packet opens an Ogg stream, and `simd` to run
// the SIMD build.
//   node tests/wasm/smoke.mjs [ogg] [simd]
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');
const simd = process.argv.includes('simd');
const { default: createMockingbirdModule } = await import(path.join(root, simd ? 'build-wasm-simd' : 'build-wasm', 'mockingbird.js'));
const m = await createMockingbirdModule();

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

const ogg = process.argv.includes('ogg');
const codec = ogg ? 3 : 1;
const F = { backend: 0, enableRnnoise: 3, enableVad: 4, codec: 19, pcmRingFrames: 23 };
const cfg = m._mockingbird_web_config_new();
m._mockingbird_web_config_set(cfg, F.backend, 4);
m._mockingbird_web_config_set(cfg, F.enableVad, 1);
m._mockingbird_web_config_set(cfg, F.enableRnnoise, 1);
m._mockingbird_web_config_set(cfg, F.codec, codec);
m._mockingbird_web_config_set(cfg, F.pcmRingFrames, 1 << 16);
const statusPtr = m._malloc(4);
const engine = m._mockingbird_web_create(cfg, statusPtr);
const createStatus = m.HEAP32[statusPtr >> 2];
m._mockingbird_web_config_free(cfg);
if (!engine) throw new Error(`mockingbird_create failed: ${createStatus}`);
if (m._mockingbird_start(engine) !== 0) throw new Error('mockingbird_start failed');

const pcm = wavSamples(path.join(root, 'tests', 'fixtures', 'hello.wav'));
const chunk = 320;
const buf = m._malloc(chunk * 2);
const evt = m._mockingbird_web_event_new();
const pkt = m._mockingbird_web_packet_new();
const counts = {};
let packets = 0, bytes = 0, last = 0, badStart = 0;
function take() {
  while (m._mockingbird_drain(engine, pkt) === 0) {
    packets++;
    const size = m._mockingbird_web_packet_get(pkt, 5);
    const flags = m._mockingbird_web_packet_get(pkt, 3);
    bytes += size;
    if (flags & 8) last++;
    if (m._mockingbird_web_packet_get(pkt, 4) !== codec) badStart++;
    if (ogg && (flags & 4)) {
      const at = m._mockingbird_web_packet_get(pkt, 6);
      if (String.fromCharCode(...m.HEAPU8.subarray(at, at + 4)) !== 'OggS') badStart++;
    }
    m._mockingbird_release(engine, pkt);
  }
}
const t0 = performance.now();
for (let i = 0, ts = 0; i + chunk <= pcm.length; i += chunk, ts += 20000) {
  m.HEAP16.set(pcm.subarray(i, i + chunk), buf >> 1);
  if (m._mockingbird_web_push_pcm(engine, buf, chunk, ts) !== 0) throw new Error('push failed');
  while (m._mockingbird_poll_event(engine, evt) === 0) {
    const type = m._mockingbird_web_event_get(evt, 0);
    counts[type] = (counts[type] ?? 0) + 1;
  }
  take();
}
const ms = performance.now() - t0;
m._mockingbird_stop(engine);
take();
while (m._mockingbird_poll_event(engine, evt) === 0) {
  const type = m._mockingbird_web_event_get(evt, 0);
  counts[type] = (counts[type] ?? 0) + 1;
}
const stats = m._mockingbird_web_stats_new();
m._mockingbird_get_stats(engine, stats);
const captured = m._mockingbird_web_stats_get(stats, 0);
const dropped = m._mockingbird_web_stats_get(stats, 1);
const rms = m._mockingbird_web_stats_get(stats, 8);
m._mockingbird_web_stats_free(stats);
m._mockingbird_web_event_free(evt);
m._mockingbird_web_packet_free(pkt);
m._free(buf);
m._free(statusPtr);
m._mockingbird_destroy(engine);

const seconds = pcm.length / 16000;
console.log(JSON.stringify({ version: m.UTF8ToString(m._mockingbird_version_string()), simd, codec, seconds, captured, dropped, rms, packets, bytes, last, events: counts, cpu_share: ms / 1000 / seconds }));
if (!(counts[1] >= 1 && counts[4] >= 1 && packets > 0 && dropped === 0 && last >= 1 && badStart === 0)) {
  console.error('expected speech and segment events, LAST packets, the right codec and no drops');
  process.exit(1);
}
console.log('wasm smoke: PASS');
