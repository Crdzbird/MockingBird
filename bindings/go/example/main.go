// Command auris-demo is a flag-driven tour of the Auris engine. It runs any
// combination of backend (live mic or a WAV file via HOST_PCM), VAD tier,
// anomaly gate, YAMNet sound classification, RNNoise denoise, and codec — plus
// named -presets that bundle common recipes. It prints detections, an
// encoded-packet tally, and final stats.
//
//	go run .                                   # default mic, fast VAD + anomaly
//	go run . -preset everything                # neural VAD + anomaly + YAMNet + RNNoise
//	go run . -preset voice -rnnoise on         # just denoised human voice
//	go run . -file ../../../tests/fixtures/hello.wav -vad neural -codec opus
//	go run . -sensitivity 0.8 -soundclass on   # one dial, everything eager
package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"os/signal"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"flag"

	"github.com/auris-audio/auris"
)

func main() {
	var (
		preset  = flag.String("preset", "", "recipe: voice | everything | anomaly | denoised-voice | music")
		backend = flag.String("backend", "", "auto | mic | hostpcm  (default: auto, or hostpcm with -file)")
		file    = flag.String("file", "", "16-bit mono WAV to process via HOST_PCM")
		vad     = flag.String("vad", "", "off | fast | neural")
		anomaly = flag.String("anomaly", "", "on | off")
		sound   = flag.String("soundclass", "", "on | off")
		rnnoise = flag.String("rnnoise", "", "on | off")
		codec   = flag.String("codec", "", "pcm | opus")
		sens    = flag.Float64("sensitivity", -1, "0..1 dial driving every detector; <0 uses explicit flags")
		sileroF = flag.String("neural-model", "", "path to silero_vad.onnx")
		yamnetF = flag.String("yamnet-model", "", "path to yamnet_3s.onnx")
		seconds = flag.Int("seconds", 0, "mic capture seconds; 0 = until Ctrl-C")
		adapt   = flag.String("adaptive", "", "on | off: shed YAMNet work under load (default: on for mic, off for -file)")
		gate    = flag.Float64("gate", 0, "silence gate in dBFS (0 = library default -60; -120 disables)")
	)
	flag.Parse()

	silero := firstNonEmpty(*sileroF, locateModel("silero_vad.onnx"))
	yamnet := firstNonEmpty(*yamnetF, locateModel("yamnet_3s.onnx"))
	labels := loadLabels() // optional YAMNet class names

	cfg := buildConfig(*preset, *sens, silero, yamnet)
	applyOverrides(&cfg, *backend, *file, *vad, *anomaly, *sound, *rnnoise, *codec, silero, yamnet)
	sanitize(&cfg, silero, yamnet)

	// Adaptive degradation reacts to wall-clock load, so it belongs on the live
	// mic path only: an offline file run should always produce full output.
	cfg.EnableAdaptiveDegradation = cfg.Backend != auris.BackendHostPCM && *file == ""
	if *adapt != "" {
		cfg.EnableAdaptiveDegradation = *adapt == "on"
	}
	if *gate != 0 {
		cfg.SilenceGateDbfs = float32(*gate)
	}

	fmt.Printf("Auris %s — %s\n", auris.Version(), describe(cfg, *preset))

	e, err := auris.New(cfg)
	if err != nil {
		fmt.Fprintln(os.Stderr, "create:", err)
		os.Exit(1)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "start:", err)
		os.Exit(1)
	}

	if *file != "" || cfg.Backend == auris.BackendHostPCM {
		runFile(e, *file, labels)
	} else {
		runMic(e, *seconds, labels)
	}
}

// --- run modes ---------------------------------------------------------------

func runMic(e *auris.Engine, seconds int, labels []string) {
	fmt.Println("listening on the default mic — Ctrl-C to stop")
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt)
	var deadline <-chan time.Time
	if seconds > 0 {
		deadline = time.After(time.Duration(seconds) * time.Second)
	}
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()
	var pkts, bytesN int
	for {
		select {
		case ev := <-e.Events():
			printEvent(ev, labels)
		case p := <-e.Packets():
			pkts++
			bytesN += len(p.Data)
		case <-ticker.C:
			st, _ := e.Stats()
			fmt.Printf("   … %d frames · %d packets (%d B to host) · %d segments\n",
				st.FramesCaptured, pkts, bytesN, st.SegmentsOpened)
		case <-sig:
			report(e, pkts, bytesN)
			return
		case <-deadline:
			report(e, pkts, bytesN)
			return
		}
	}
}

func runFile(e *auris.Engine, path string, labels []string) {
	if path == "" {
		fmt.Fprintln(os.Stderr, "hostpcm mode needs -file <wav>")
		os.Exit(2)
	}
	samples, rate, err := readWAV(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, "wav:", err)
		os.Exit(1)
	}
	fmt.Printf("processing %s (%d samples @ %d Hz)\n", filepath.Base(path), len(samples), rate)
	frame := int(rate/1000) * 20 // 20 ms
	if frame == 0 {
		frame = 320
	}
	var pkts, bytesN int
	drain := func() {
		for {
			select {
			case ev := <-e.Events():
				printEvent(ev, labels)
			case p := <-e.Packets():
				pkts++
				bytesN += len(p.Data)
			default:
				return
			}
		}
	}
	for off := 0; off+frame <= len(samples); off += frame {
		_ = e.PushPCM(samples[off:off+frame], uint64(off/frame)*20_000)
		if (off/frame)%16 == 15 {
			time.Sleep(15 * time.Millisecond)
			drain()
		}
	}
	time.Sleep(250 * time.Millisecond)
	drain()
	_ = e.Stop()
	drain()
	report(e, pkts, bytesN)
}

func report(e *auris.Engine, pkts, bytesN int) {
	st, _ := e.Stats()
	de, dp := e.Dropped()
	fmt.Printf("\n── summary ──\n"+
		"frames captured : %d\n"+
		"segments        : %d opened / %d closed\n"+
		"packets to host : %d (%d bytes encoded)\n"+
		"native dropped  : %d frames, %d packets  |  host-side dropped: %d events, %d packets\n"+
		"worker          : %.2f%% busy (last window), max frame %d µs, %d wakeups\n"+
		"work shed       : YAMNet %d ran / %d skipped, RNNoise skipped %d quiet frames, degradation level %d\n",
		st.FramesCaptured, st.SegmentsOpened, st.SegmentsClosed,
		pkts, bytesN, st.FramesDroppedCapture, st.PacketsDropped, de, dp,
		float64(st.WorkerBusyPpm)/1e4, st.WorkerMaxFrameUs, st.WorkerWakeups,
		st.SoundClassInferences, st.SoundClassInferencesSkipped, st.PreprocFramesSkipped, st.DegradationLevel)
}

func printEvent(ev auris.Event, labels []string) {
	switch ev.Type {
	case auris.EventSpeechBegin:
		fmt.Println("voice   ▶ speech begin")
	case auris.EventSpeechEnd:
		fmt.Println("voice   ■ speech end")
	case auris.EventAnomaly:
		fmt.Printf("anomaly   reason=%d conf=%.2f\n", ev.ReasonMask, ev.Confidence)
	case auris.EventSoundClass:
		cls := ev.SoundClass()
		name := ""
		if cls >= 0 && cls < len(labels) {
			name = " " + labels[cls]
		}
		fmt.Printf("sound   [%s]%s (class %d, conf %.2f)\n",
			auris.CategoryForYAMNetClass(cls), name, cls, ev.Confidence)
	case auris.EventDegradation:
		fmt.Printf("engine  ⚠ degradation level %d (worker %.0f%% busy) — shedding YAMNet work\n",
			ev.DegradationLevel(), ev.Confidence*100)
	case auris.EventBackendError:
		fmt.Fprintf(os.Stderr, "backend error: %v — a detector's model may be missing or "+
			"libauris was built without ONNX Runtime\n", ev.BackendError())
	}
}

// --- config building ---------------------------------------------------------

func buildConfig(preset string, sens float64, silero, yamnet string) auris.Config {
	switch preset {
	case "voice": // just human voice, denoised, Opus
		c := auris.Sensitive(0.6, auris.WithVadTier(auris.VadNeural),
			auris.WithNeuralVADModel(silero), auris.WithRNNoise(true))
		c.EnableAnomaly, c.EnableSoundClass, c.Codec = false, false, auris.CodecOpus
		return c
	case "everything": // every detector, neural VAD, denoised, Opus
		c := auris.Sensitive(0.6, auris.WithNeuralVADModel(silero), auris.WithSoundClassModel(yamnet),
			auris.WithRNNoise(true))
		c.Codec = auris.CodecOpus
		return c
	case "anomaly": // watch for unusual sounds; voice off, eager
		c := auris.DefaultConfig()
		c.EnableVAD, c.EnableAnomaly = false, true
		c.AnomalyRMSZ, c.AnomalyFluxZ, c.Codec = 2.0, 2.0, auris.CodecOpus
		return c
	case "denoised-voice": // RNNoise + neural VAD, no anomaly
		c := auris.DefaultConfig()
		c.EnableRNNoise, c.VadTier, c.NeuralVADModelPath = true, auris.VadNeural, silero
		c.EnableAnomaly, c.Codec = false, auris.CodecOpus
		return c
	case "music": // classify ambient sound/music; voice + anomaly off
		c := auris.DefaultConfig()
		c.EnableVAD, c.EnableAnomaly = false, false
		c.EnableSoundClass, c.SoundClassModelPath, c.SoundClassThreshold = true, yamnet, 0.3
		return c
	default:
		if sens >= 0 {
			return auris.Sensitive(sens, auris.WithNeuralVADModel(silero), auris.WithSoundClassModel(yamnet))
		}
		return auris.DefaultConfig() // fast VAD + anomaly
	}
}

func applyOverrides(cfg *auris.Config, backend, file, vad, anomaly, sound, rnnoise, codec, silero, yamnet string) {
	if file != "" {
		cfg.Backend = auris.BackendHostPCM
	}
	switch backend {
	case "mic":
		cfg.Backend = auris.BackendAuto
	case "hostpcm":
		cfg.Backend = auris.BackendHostPCM
	case "auto":
		cfg.Backend = auris.BackendAuto
	}
	switch vad {
	case "off":
		cfg.EnableVAD = false
	case "fast":
		cfg.EnableVAD, cfg.VadTier = true, auris.VadFast
	case "neural":
		cfg.EnableVAD, cfg.VadTier, cfg.NeuralVADModelPath = true, auris.VadNeural, silero
	}
	setBool(&cfg.EnableAnomaly, anomaly)
	if setBool(&cfg.EnableSoundClass, sound) && cfg.EnableSoundClass {
		cfg.SoundClassModelPath = yamnet
	}
	setBool(&cfg.EnableRNNoise, rnnoise)
	switch codec {
	case "pcm":
		cfg.Codec = auris.CodecPCMS16LE
	case "opus":
		cfg.Codec = auris.CodecOpus
	}
}

// sanitize downgrades features whose model isn't available so the demo still
// runs (with a note) instead of erroring.
func sanitize(cfg *auris.Config, silero, yamnet string) {
	if cfg.EnableVAD && cfg.VadTier == auris.VadNeural && cfg.NeuralVADModelPath == "" && silero == "" {
		fmt.Fprintln(os.Stderr, "note: no silero_vad.onnx found — falling back to fast VAD")
		cfg.VadTier = auris.VadFast
	}
	if cfg.EnableSoundClass && cfg.SoundClassModelPath == "" && yamnet == "" {
		fmt.Fprintln(os.Stderr, "note: no yamnet_3s.onnx found — disabling sound classification")
		cfg.EnableSoundClass = false
	}
	// RNNoise needs an integer 48 kHz ratio (8/16/48 kHz).
	if cfg.EnableRNNoise && 48000%cfg.SampleRateHz != 0 {
		fmt.Fprintln(os.Stderr, "note: RNNoise needs an 8/16/48 kHz rate — disabling")
		cfg.EnableRNNoise = false
	}
}

func describe(c auris.Config, preset string) string {
	var on []string
	if c.EnableVAD {
		tier := "fast"
		if c.VadTier == auris.VadNeural {
			tier = "neural"
		}
		on = append(on, "VAD:"+tier)
	}
	if c.EnableAnomaly {
		on = append(on, "anomaly")
	}
	if c.EnableSoundClass {
		on = append(on, "YAMNet")
	}
	if c.EnableRNNoise {
		on = append(on, "RNNoise")
	}
	be := map[auris.Backend]string{auris.BackendAuto: "mic", auris.BackendHostPCM: "host-pcm"}[c.Backend]
	cd := map[auris.Codec]string{auris.CodecPCMS16LE: "pcm", auris.CodecOpus: "opus"}[c.Codec]
	tag := ""
	if preset != "" {
		tag = "preset=" + preset + " · "
	}
	return fmt.Sprintf("%s%s · %d Hz · %s · [%s]", tag, be, c.SampleRateHz, cd, strings.Join(on, " "))
}

// --- helpers -----------------------------------------------------------------

func setBool(dst *bool, v string) bool {
	switch v {
	case "on", "true", "1":
		*dst = true
		return true
	case "off", "false", "0":
		*dst = false
		return true
	}
	return false
}

func firstNonEmpty(a, b string) string {
	if a != "" {
		return a
	}
	return b
}

// locateModel looks for a bundled model next to this source file's repo.
func locateModel(name string) string {
	_, self, _, ok := runtime.Caller(0)
	if !ok {
		return ""
	}
	// bindings/go/example/main.go -> repo/tests/fixtures/<name>
	p := filepath.Join(filepath.Dir(self), "..", "..", "..", "tests", "fixtures", name)
	if _, err := os.Stat(p); err == nil {
		return p
	}
	return ""
}

func loadLabels() []string {
	p := locateModel("yamnet_class_map.txt")
	if p == "" {
		return nil
	}
	b, err := os.ReadFile(p)
	if err != nil {
		return nil
	}
	var out []string
	for _, line := range strings.Split(string(b), "\n") {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		if i := strings.IndexByte(line, ' '); i >= 0 {
			line = line[i+1:]
		}
		out = append(out, line)
	}
	return out
}

func readWAV(path string) (samples []int16, rate uint32, err error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, 0, err
	}
	if len(b) < 12 || string(b[0:4]) != "RIFF" || string(b[8:12]) != "WAVE" {
		return nil, 0, fmt.Errorf("not a WAV file")
	}
	i := 12
	for i+8 <= len(b) {
		id := string(b[i : i+4])
		sz := int(binary.LittleEndian.Uint32(b[i+4 : i+8]))
		body := b[i+8:]
		if len(body) < sz {
			break
		}
		switch id {
		case "fmt ":
			rate = binary.LittleEndian.Uint32(body[4:8])
		case "data":
			samples = make([]int16, sz/2)
			for k := range samples {
				samples[k] = int16(binary.LittleEndian.Uint16(body[k*2 : k*2+2]))
			}
			return samples, rate, nil
		}
		if sz%2 == 1 {
			sz++
		}
		i += 8 + sz
	}
	return nil, 0, fmt.Errorf("no data chunk")
}
