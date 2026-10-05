// Runs the WebAssembly build over the same fixture as the Flutter apps and
// records the time the push loop takes. The web build processes each chunk
// inline on the caller's thread, so wall time of the loop is its CPU time.
//   node bench/flutter_compare/web_bench.mjs
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');
const { default: createAurisModule } = await import(path.join(root, 'packages', 'mockingbird_web', 'assets', 'auris.js'));
const m = await createAurisModule();

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

function fixture() {
  const clips = ['hello.wav', 'music.wav', 'miaow_16k.wav'].map((n) => wavSamples(path.join(root, 'tests', 'fixtures', n)));
  const gap = new Int16Array(16000);
  const parts = [];
  let total = 0;
  while (total < 60 * 16000) {
    for (const c of clips) { parts.push(c, gap); total += c.length + gap.length; }
  }
  const out = new Int16Array(total);
  let pos = 0;
  for (const p of parts) { out.set(p, pos); pos += p.length; }
  return out;
}

const F = { backend: 0, enableRnnoise: 3, enableVad: 4, enableAnomaly: 9, codec: 19, pcmRingFrames: 23, packetQueueCapacity: 24, silenceGate: 27 };
const scenarios = [
  ['vad_fast', { [F.enableVad]: 1, [F.enableAnomaly]: 0 }],
  ['anomaly', { [F.enableVad]: 0, [F.enableAnomaly]: 1 }],
  ['denoise', { [F.enableVad]: 0, [F.enableAnomaly]: 0, [F.enableRnnoise]: 1, [F.silenceGate]: -120 }],
  ['everything_web', { [F.enableVad]: 1, [F.enableAnomaly]: 1, [F.enableRnnoise]: 1, [F.codec]: 1 }],
  ['everything_web_no_gate', { [F.enableVad]: 1, [F.enableAnomaly]: 1, [F.enableRnnoise]: 1, [F.codec]: 1, [F.silenceGate]: -120 }],
];

const pcm = fixture();
const chunk = 320;
const buf = m._malloc(chunk * 2);
const evt = m._auris_web_event_new();
const pkt = m._auris_web_packet_new();
const stats = m._auris_web_stats_new();
const statusPtr = m._malloc(4);
mkdirSync(path.join(here, 'results'), { recursive: true });
const lines = [];
for (const [name, fields] of scenarios) {
  const cfg = m._auris_web_config_new();
  m._auris_web_config_set(cfg, F.backend, 4);
  m._auris_web_config_set(cfg, F.pcmRingFrames, 1 << 17);
  m._auris_web_config_set(cfg, F.packetQueueCapacity, 4096);
  for (const [k, v] of Object.entries(fields)) m._auris_web_config_set(cfg, Number(k), v);
  const engine = m._auris_web_create(cfg, statusPtr);
  m._auris_web_config_free(cfg);
  if (!engine) throw new Error(`${name}: create failed ${m.HEAP32[statusPtr >> 2]}`);
  m._auris_start(engine);
  let packets = 0, speech = 0, segments = 0;
  const rss0 = process.memoryUsage().rss;
  let rssPeak = rss0;
  const t0 = performance.now();
  for (let i = 0, ts = 0, n = 0; i + chunk <= pcm.length; i += chunk, ts += 20000, n++) {
    m.HEAP16.set(pcm.subarray(i, i + chunk), buf >> 1);
    m._auris_web_push_pcm(engine, buf, chunk, ts);
    while (m._auris_poll_event(engine, evt) === 0) {
      const t = m._auris_web_event_get(evt, 0);
      if (t === 1) speech++;
      if (t === 4) segments++;
    }
    while (m._auris_drain(engine, pkt) === 0) { packets++; m._auris_release(engine, pkt); }
    if (n % 250 === 0) rssPeak = Math.max(rssPeak, process.memoryUsage().rss);
  }
  const seconds = (performance.now() - t0) / 1000;
  m._auris_stop(engine);
  m._auris_get_stats(engine, stats);
  const dropped = m._auris_web_stats_get(stats, 1);
  m._auris_destroy(engine);
  const audio = pcm.length / 16000;
  const r = { library: 'mockingbird_web', scenario: name, audio_s: audio, cpu_s: seconds, wall_s: seconds, cpu_share: seconds / audio, rss_base_mb: rss0 / 1048576, rss_peak_mb: rssPeak / 1048576, counters: { frames_pushed: Math.floor(pcm.length / chunk), packets, speech_begin: speech, segments, frames_dropped_capture: dropped } };
  lines.push(JSON.stringify(r));
  console.log(`${name.padEnd(24)} share ${(r.cpu_share * 100).toFixed(2)}%  packets ${packets} segments ${segments} dropped ${dropped}`);
}
writeFileSync(path.join(here, 'results', 'mockingbird_web.jsonl'), lines.join('\n') + '\n');
