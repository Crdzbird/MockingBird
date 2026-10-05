// Collects the input into fixed chunks of int16 samples at the engine's rate,
// resampling linearly when the AudioContext could not open at that rate.
class MockingbirdCaptureProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions || {};
    this.ratio = (o.inputRate || sampleRate) / (o.outputRate || sampleRate);
    this.chunk = o.chunk || 320;
    this.out = new Int16Array(this.chunk);
    this.fill = 0;
    this.pos = 0;
    this.carry = new Float32Array(0);
  }

  process(inputs) {
    const channel = inputs[0] && inputs[0][0];
    if (!channel) return true;
    const data = this.carry.length
      ? Float32Array.of(...this.carry, ...channel)
      : channel;
    let i = this.pos;
    while (i + 1 < data.length) {
      const k = Math.floor(i);
      const frac = i - k;
      const v = data[k] + (data[k + 1] - data[k]) * frac;
      this.out[this.fill++] = Math.max(-32768, Math.min(32767, Math.round(v * 32768)));
      if (this.fill === this.chunk) {
        this.port.postMessage(this.out, [this.out.buffer]);
        this.out = new Int16Array(this.chunk);
        this.fill = 0;
      }
      i += this.ratio;
    }
    const k = Math.floor(i);
    this.carry = data.slice(Math.min(k, data.length));
    this.pos = i - k;
    return true;
  }
}

registerProcessor('mockingbird-capture', MockingbirdCaptureProcessor);
